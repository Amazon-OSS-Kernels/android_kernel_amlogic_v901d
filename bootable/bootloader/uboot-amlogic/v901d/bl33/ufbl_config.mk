

### UFBL flags
UFBL_FLAGS += -I$(buildtree)/include2
UFBL_FLAGS += -I$(buildtree)/include
UFBL_FLAGS += -I$(buildsrc)/include
UFBL_FLAGS += -D__KERNEL__ -D__HAVE_ARCH_BCOPY

### UFBL config
# DEFINES was defined in ufbl
UFBL_FLAGS += $(DEFINES)
#
# # Allow boards to use custom optimize flags on a per dir/file basis
BCURDIR = $(subst $(SRCTREE)/,,$(CURDIR:$(obj)%=%))
ALL_AFLAGS = $(AFLAGS) $(AFLAGS_$(BCURDIR)/$(@F)) $(AFLAGS_$(BCURDIR))
ALL_CFLAGS = $(CFLAGS) $(CFLAGS_$(BCURDIR)/$(@F)) $(CFLAGS_$(BCURDIR)) $(UFBL_FLAGS)

### UFBL config
$(obj)%.s:	%.S
	@mkdir -p $(@D)
	$(CPP) $(ALL_AFLAGS) -o $@ $<
$(obj)%.o:	%.S
	@mkdir -p $(@D)
	$(CC)  $(ALL_AFLAGS) -o $@ $< -c
$(obj)%.o:	%.c
	@mkdir -p $(@D)
	$(CC)  $(ALL_CFLAGS) -o $@ $< -c
$(obj)%.i:	%.c
	@mkdir -p $(@D)
	$(CPP) $(ALL_CFLAGS) -o $@ $< -c
$(obj)%.s:	%.c
	@mkdir -p $(@D)
	$(CC)  $(ALL_CFLAGS) -o $@ $< -c -S

# add for old uboot version compatible
cmd_link_o_target = $(if $(strip $1),\
		    $(LD) $(LDFLAGS) -r -o $@ $1,\
		    rm -f $@; $(AR) rcs $@ )
