# TODO #

# Issues In Progress #

## Tyler ##

## Jacob ##

## Julian ##


# Completed #
- [1] Copy all regular files from the working directory to the .backup directory\
- [2] Compare last modification times of duplicate files before overwriting, and inform the user\
- [3] Create a thread to copy each file
- [4] Perform a restore operation when the -r flag is specified\
- [5] Compare last modification times of duplicate files before restoring, and inform the user
- [6] Create a thread to restore each file
- [7] Modify copy to work recursively on directories
- [8] Update to use the cwd instead of /testdir
- [9] Make sure the restoreThread closes the FP
- [10] Make sure the recursiveRestore closes the backupDir
- [11] use a mutex on printf("Copied %d files (%d bytes)\n",successfulFiles, totalBytes); since it is shared
- [12] Fix memory leaks caused by strok in restoreThread
- [13] Add comments to each method
- [14] Modify restore to work recursively on directories
- [15] fix misc valgrind issues
- [16] copyFile needs to be able to create directories
- [17] make sure all methods are being error checked, malloc, pthread funcs, etc
- [22] error, running ./BackItUp -r before a .backup directory exists causes a seg fault
- [18] the canCopy check in recursiveRestore needs to be moved to the thread, right now it is in the main thread. See the sample output in the PDF. a `### TODO` is appended to the output
- [19] error, Multiple threads restore the same file
- [20] hidden files (such as .hidden.txt) don't get copied and may not get restored
- [21] error, files with no permissions (chmod 000 file.txt) exit the program early 
- [22] clean up extra messages
- [23] fix issue where it is unable to copy some files, problem is at line 509
- [24] test project on the school lx server
- [25] make sure DEBUG is off, run `make zip`, and submit!

## Completed Tests ##
- [t2] Test backing up large files, csv, and image files
- [t3] Test empty files

