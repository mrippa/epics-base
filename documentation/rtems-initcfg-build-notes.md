# RTEMS Init-Configuration Build Notes

This note tracks the current EPICS-side prototype for bundling host-side
configuration files into an RTEMS boot image.

## Summary

The current flow is:

1. Declare one or more `host-path:target-path` mappings in EPICS make/config
   files.
2. Generate any required host-side files under `O.$(T_A)`.
3. Stage those files into a target-shaped directory tree.
4. Package that staged tree as a tarball.
5. Hand that tarball to the existing RTEMS boot-image path.
6. RTEMS-side startup code can later unpack the embedded content into IMFS.

## What Lives Where

There are two sides to this.

1. EPICS provides the configuration and build mechanics to describe files,
   generate them if needed, stage them, and produce a build artifact such as a
   tarball.
2. RTEMS-side startup code consumes that packaged artifact, unpacks it into
   IMFS, and uses the files during early RTEMS init before normal IOC startup
   and before networking is fully configured.

So the configuration is primarily EPICS-side, but we end up with BSP config
archives linked into the target executable. The build artifacts start life in
EPICS paths, then one or more of them can be folded into the RTEMS executable
or boot image.

### EPICS side

EPICS is the natural place to define:

- which files are needed before networking
- where those files live on the host
- what their target paths should be in the RTEMS filesystem
- how to generate host-side files if the inputs are not static
- how to stage them into a temporary tree
- how to generate a tarball or similar packaged artifact

Typical EPICS-side locations for the mvme2700 prototype:

- `configure/os/CONFIG.Common.RTEMS`
- `configure/os/CONFIG.Common.RTEMS-mvme2700`
- `configure/os/CONFIG.<host>.RTEMS-mvme2700`
- `configure/os/CONFIG_SITE.Common.RTEMS-mvme2700`
- `O.<arch>/...` for generated files and staging outputs

### RTEMS side

RTEMS is the natural place to define:

- how the packaged artifact is linked into the executable
- which symbol names have the packaged content
- when the startup code unpacks it into IMFS
- what happens before network bring-up vs after network bring-up

Current understanding from the RTEMS side:

- convert tarball to C or object input
- link into the target executable
- unpack into IMFS during early init

## Current Prototype in Base

The current prototype lives in:

- `configure/os/CONFIG.Common.RTEMS`
- `configure/os/CONFIG.Common.RTEMS-mvme2700`

The common RTEMS file now contains both the variable interface and the generic
staging/packaging rule.

In `configure/os/CONFIG.Common.RTEMS` the interface is:

```make
RTEMS_INITCFG_ENABLE ?= NO
RTEMS_INITCFG_MAPPINGS ?=
RTEMS_INITCFG_GEN_ROOT ?= O.$(T_A)/rtems-initcfg-gen
RTEMS_INITCFG_GEN_FILES ?=
RTEMS_INITCFG_STAGING_ROOT ?= O.$(T_A)/rtems-initcfg
RTEMS_INITCFG_TAR ?= $(RTEMS_INITCFG_STAGING_ROOT).tar
RTEMS_INITCFG_TAR_CMD ?= tar
```

When `RTEMS_INITCFG_ENABLE` is `YES`, the generic rule:

1. builds the generated host-side config files
2. applies each `host-path:target-path` mapping into the staging tree
3. packages the staged tree as `$(RTEMS_INITCFG_TAR)`

The common file also currently defines a lightweight convenience target:

```make
.PHONY: rtems-initcfg
rtems-initcfg: $(RTEMS_INITCFG_TAR)
```

This is only a manual entry point. The normal RTEMS boot-image build path still
hooks in through `MUNCH_DEPENDS`.

## mvme2700 Prototype

In `configure/os/CONFIG.Common.RTEMS-mvme2700` the current declarations are:

```make
RTEMS_INITCFG_ENABLE ?= YES
RTEMS_MVME2700_RC_CONF ?= $(RTEMS_INITCFG_GEN_ROOT)/mvme2700/rc.conf
RTEMS_INITCFG_GEN_FILES += $(RTEMS_MVME2700_RC_CONF)
RTEMS_INITCFG_MAPPINGS += $(RTEMS_MVME2700_RC_CONF):/etc/rc.conf
MUNCH_DEPENDS += $(RTEMS_INITCFG_TAR)
```

This means:

- the mvme2700 build enables the generic initcfg packaging interface
- `rc.conf` is treated as a generated host-side file
- that file is staged to `/etc/rc.conf` in the target image
- the tarball is pulled into the normal RTEMS boot-image path

So application boot builds should pick this up automatically, and a manual
`make rtems-initcfg` should also exercise the packaging path directly.

## Temporary Placeholder Rule

For the moment there is also a temporary mvme2700-specific fallback rule in
`configure/os/CONFIG.Common.RTEMS`:

```make
$(RTEMS_MVME2700_RC_CONF):
	@$(MKDIR) $(dir $@)
	@$(RM) $@
	@echo "# Generated mvme2700 RTEMS init configuration placeholder" > $@
	@echo "# Replace or extend this rule with application or site policy." >> $@
```

That rule is intentionally only a prototype convenience. It keeps the packaging
path working until a real application or site-specific `rc.conf` generation rule
is in place.

This placement is slightly awkward because it is board-specific rule logic in a
generic RTEMS config file, but it was useful to prove the concept with minimal
churn.

## Host- and Site-Specific Variants

If there is host-specific tool choice, then use
`configure/os/CONFIG.<host>.RTEMS-mvme2700`, for example:

```make
# configure/os/CONFIG.linux-x86_64.RTEMS-mvme2700
RTEMS_INITCFG_TAR_CMD = tar
```

```make
# configure/os/CONFIG.darwin-aarch64.RTEMS-mvme2700
RTEMS_INITCFG_TAR_CMD = gtar
```

If the actual file mappings are site-local and should not be checked in as
target defaults, then move only the mapping lines into
`configure/os/CONFIG_SITE.Common.RTEMS-mvme2700` or
`configure/os/CONFIG_SITE.<host>.RTEMS-mvme2700`.

## What This Does Not Decide Yet

The current prototype proves the mechanics, but it does not yet settle the
longer-term application-side contract.

Open questions:

- where application rules should generate `rc.conf`
- what the cleanest application-facing interface looks like
- whether the current Base-side prototype is enough
- whether a later structural cleanup such as `RULES.Common.$(OS_CLASS)` is
  justified

## Next Step

The next useful step is to create a separate test application/module with
`makeBaseApp.pl` and exercise this from the application side.

That should answer:

- how application code wants to declare mappings
- where application rules want to generate `rc.conf`
- whether `make`, `make rtems-initcfg`, and `make clean` behave cleanly
- whether the current Base-side prototype should stay lightweight or grow into a
  more formal rules-layer extension
