# Makefile for hev-lib
 
PP=cpp
CC=cc
LD=ld
AR=ar
CCFLAGS=-g -Werror -Wall
LDFLAGS=
 
SRCDIR=src
BINDIR=bin
BUILDDIR=build
 
STATIC_TARGET=$(BINDIR)/libhev-lib.a
SHARED_TARGET=$(BINDIR)/libhev-lib.so

$(SHARED_TARGET) : CCFLAGS+=-fPIC
$(SHARED_TARGET) : LDFLAGS+=-shared -pthread

CCOBJSFILE=$(BUILDDIR)/ccobjs
-include $(CCOBJSFILE)
LDOBJS=$(patsubst $(SRCDIR)%.c,$(BUILDDIR)%.o,$(CCOBJS))
 
DEPEND=$(LDOBJS:.o=.dep)
 
shared : $(CCOBJSFILE) $(SHARED_TARGET)
	@$(RM) $(CCOBJSFILE)
 
static : $(CCOBJSFILE) $(STATIC_TARGET)
	@$(RM) $(CCOBJSFILE)
 
clean : 
	@echo -n "Clean ... " && $(RM) $(BINDIR)/* $(BUILDDIR)/* && echo "OK"
 
$(CCOBJSFILE) : 
	@echo CCOBJS=`ls $(SRCDIR)/*.c` > $(CCOBJSFILE)
 
$(STATIC_TARGET) : $(LDOBJS)
	@echo -n "Linking $^ to $@ ... " && $(AR) csq $@ $^ && echo "OK"
 
$(SHARED_TARGET) : $(LDOBJS)
	@echo -n "Linking $^ to $@ ... " && $(CC) -o $@ $^ $(LDFLAGS) && echo "OK"
 
$(BUILDDIR)/%.dep : $(SRCDIR)/%.c
	@$(PP) $(CCFLAGS) -MM -MT $(@:.dep=.o) -o $@ $<
 
$(BUILDDIR)/%.o : $(SRCDIR)/%.c
	@echo -n "Building $< ... " && $(CC) $(CCFLAGS) -c -o $@ $< && echo "OK"
 
-include $(DEPEND)

