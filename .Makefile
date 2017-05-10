CC=gcc
VERSIONFILE=src/version
LOCKFILE=bin/.version
OBJE=src/base_daemon.o \
	src/log_daemon.o \
	src/public_base.o \
	src/service.o \
	src/socket_server.o \
	src/mysql_talk.o \
	src/scmessctl.o \
	src/file_msg.o \
	src/pthread_pool.o

OBJDIR=/usr/local/share/FileManagerServer
tmpdir=`pwd`

EXEC=bin/flm-server

WALL= -lpthread -lmysqlclient -lrt

all:$(EXEC)

$(EXEC):$(OBJE)
	$(CC) $(OBJE) $(WALL) -o $@
	cp $(VERSIONFILE) $(LOCKFILE)
	@echo "Debug Sucess"

clean:
	rm -rf src/*.o
	@echo "Clean Sucess"

install:
	cp -R $(tmpdir) /usr/local/share
	useradd -d $(OBJDIR) -M -s /sbin/nologin saligia
	chown -R saligia:saligia $(OBJDIR)
	cp bin/safile /etc/init.d/
	chmod 0722 /etc/init.d/safile
