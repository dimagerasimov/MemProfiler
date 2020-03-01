CC := gcc
CXX := g++
CPP11FLAGS := -std=c++11

OUTDIR := ..

all: tiny small cutie looker weirdo performer

clean:
	rm $(OUTDIR)/tiny
	rm $(OUTDIR)/small
	rm $(OUTDIR)/cutie
	rm $(OUTDIR)/looker
	rm $(OUTDIR)/weirdo
	rm $(OUTDIR)/performer

tiny:
	$(CC) $@.c -o $(OUTDIR)/$@

small:
	$(CXX) $@.cpp -o $(OUTDIR)/$@

cutie:
	$(CXX) $@.cpp -o $(OUTDIR)/$@

looker:
	$(CC) $@.c -o $(OUTDIR)/$@

weirdo:
	$(CXX) $@.cpp -o $(OUTDIR)/$@ $(CPP11FLAGS)

performer:
	$(CXX) $@.cpp -o $(OUTDIR)/$@ $(CPP11FLAGS)
