web-all:

web_OUTDIR:=out/web
web_MIDDIR:=mid/web

web-run:$(demo_ROM) $(eggdev_EXE);$(eggdev_EXE) serve --htdocs=src/www --htdocs=out

# eggcade.html is a web-based launcher for external ROM files.
# Totally optional; blank this out to skip it.
web_EGGCADE_HTML:=$(web_OUTDIR)/eggcade.html

ifneq (,$(strip $(web_EGGCADE_HTML)))
  web-all:$(web_EGGCADE_HTML)
  web_RTFILES:=$(filter src/www/%.js,$(SRCFILES))
  web_EGGCADE_APPFILES:=$(filter src/eggcade/%.js,$(SRCFILES))
  web_EGGCADE_RT:=$(web_MIDDIR)/eggrt.js
  web_EGGCADE_APP:=$(web_MIDDIR)/eggcade.js
  $(web_EGGCADE_HTML):src/eggcade/prefix.html $(web_EGGCADE_RT) $(web_EGGCADE_APP) src/eggcade/suffix.html;$(PRECMD) cat $^ > $@
  $(web_EGGCADE_RT):$(eggdev_EXE) $(web_RTFILES);$(PRECMD) $(eggdev_EXE) minify -o$@ src/www/bootstrap.js
  $(web_EGGCADE_APP):$(eggdev_EXE) $(web_EGGCADE_APPFILES);$(PRECMD) $(eggdev_EXE) minify -o$@ src/eggcade/bootstrap.js
endif
