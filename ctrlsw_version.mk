MAJOR=0
MINOR=5
STEP=0
CTRLSW_VER?=$(shell ./setlocalversion.sh)

VERSION=$(MAJOR).$(MINOR).$(STEP)$(CTRLSW_VER)

