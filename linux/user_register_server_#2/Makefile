
##
## SKYPE test makefile
##
## Main tagrets are:
##  all:	builds whole project form the scratch with shared libraries
##  clean:	deletes all compilable objects including application
##  skypetests:	builds main application and all dependencies with shared libraries
##  static:	builds main application from the scratch with ststic libraries linking
##  client:	builds client test application for testing purposes
##
## Application can be customized through several parameters defined by CUSTOM_CONFIG (see below)
##


APPLICATION=skypetest

# Several hardcoded values can be configured here
CUSTOM_CONFIG=

# Customize log file name
# CUSTOM_CONFIG+=-DLOG_FILENAME='"skypetest.log"'

# Customize working threads number
# CUSTOM_CONFIG+=-DQUEUE_WORKERS_NUMBER=4

# Customize delay policy (see request.h for policies description)
# CUSTOM_CONFIG+=-DDELAY_POLICY=DELAY_EXECUTION


OPTIONS=-Wall -Werror -pedantic -Wno-long-long -pthread -g

LIBS=	$(STATIC) -lboost_thread-mt \
	$(STATIC) -lboost_regex-mt \
	$(STATIC) -lpthread

OBJS=	server.o     \
	logger.o     \
	config.o     \
	queue.o      \
	connection.o \
	request.o    \
	datafile.o   \
	manager.o    \
	main.o

%.o: %.cpp *.h
	g++ $(OPTIONS) $(CUSTOM_CONFIG) -c $<

all: clean $(APPLICATION)

static:
	$(MAKE) STATIC="-static"

$(APPLICATION): $(OBJS)
	g++ $(OPTIONS) $(OBJS) $(LIBS) -o $(APPLICATION)

clean:
	rm -f $(OBJS) $(APPLICATION)

TIME=`date "+%d%m%y_%H%M"`

BACKUP_FNAME=backup_$(TIME).tar.gz

backup:
	@tar czvf $(BACKUP_FNAME) *.cpp *.h Makefile skypetest.conf data.txt
	@echo "\n"$(BACKUP_FNAME) is ready

# Rules below are for test client only

CLIENT=client

CLIENT_SRC=client.cpp

$(CLIENT): $(CLIENT_SRC)
	g++ $(CLIENT_SRC) -o $(CLIENT)

