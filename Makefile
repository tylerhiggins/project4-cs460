# Makefile for BackItUp
# cs460-project4
PROJ = cs460-project4-Team1
BACKITUP = BackItUp

.SUFFIXES: .c.o
CC = gcc
CCFLAGS = -std=c99 -pedantic -Wall -g -Werror -D_DEFAULT_SOURCE -pthread
OBJS = ${BACKITUP}.o

# the info for compiling, runs when 'make' is called
all: ${OBJS}
	${CC} ${CCFLAGS} -o ${BACKITUP} ${BACKITUP}.o

run: all
	./${BACKITUP}
c.o:
	${CC} ${CCFLAGS} -c $<

# program tests
valgrind: clean all
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./${BACKITUP}

backitup-test: ${BACKITUP}
	@echo --- Running Test 1 esh ---
	./${BACKITUP}

restore: all
	./${BACKITUP} -r
	
tree:
	tree -a testdir/

get_test:
	echo "Downloading Test Repo"
	git clone git@github.com:JulianKeller/testfiles.git

test: all
	echo ""
	echo "Backing up all files"
	./${BACKITUP}

	echo ""
	echo "Backing up all files a second time"
	./${BACKITUP}

	echo ""
	echo "Deleting testfiles/"
	rm -rf testfiles/

	echo ""
	echo "Restoring up all files"
	./${BACKITUP} -r

	echo ""
	echo "Restoring up all files second time"
	./${BACKITUP} -r

count_files:
	find . | wc -l

rm: 
	rm -r testdir/.backup

r: 
	rm testdir/a testdir/b

# deletes unneccessary files
clean:
	rm -f ${BACKITUP} ${}.o  *.o *.tgz *.zip

# make a tarball
tarball: clean
	rm -f ${PROJ}.zip ${PROJ}.tgz
	tar -czf ${PROJ}.tgz *

# make zip file excluding test files and git
# make a zip file
zip: clean
	rm -f ${PROJ}.zip ${PROJ}.tgz
	zip -r ${PROJ}.zip *.c *.h Makefile README.* -x ".gitignore"


# make a zip file including git
zip-all: clean
	rm -f ${PROJ}.zip ${PROJ}.tgz
	zip -r ${PROJ}.zip *

# can add more *.o declarations here if necessary
${BACKITUP}.o: ${BACKITUP}.c ${BACKITUP}.h ${BACKITUP}.h