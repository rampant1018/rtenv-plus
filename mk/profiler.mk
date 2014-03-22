PROFILE = profile
LOG = syslog

profiling: $(LOGDIR)/$(LOG) $(OUTDIR)/$(TOOLDIR)/profiler
	@echo "    PROFILE "$@
	@$(OUTDIR)/$(TOOLDIR)/profiler -d $< -o $(LOGDIR)/$(PROFILE)

$(OUTDIR)/%/profiler: %/profiler.c
	@mkdir -p $(dir $@)
	@echo "    CC      "$@
	@gcc -Wall -o $@ $^
