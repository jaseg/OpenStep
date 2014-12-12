
GERBERS=export/openstep-gerbers-$(shell git describe --tags).zip


all: $(GERBERS)


$(GERBERS): $(wildcard Outputs/Gerber/*) Outputs/NCDrill/main.TXT
	zip -j $@ $^


