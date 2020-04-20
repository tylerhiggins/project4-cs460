# TODO #
- [14] Modify restore to work recursively on directories
- [8] Update to use the cwd instead of /testdir
- [9] Make sure the restoreThread closes the FP
- [10] Make sure the recursiveRestore closes the backupDir
- [11] use a mutex on printf("Copied %d files (%d bytes)\n",successfulFiles, totalBytes); since it is shared
- [12] Fix memory leaks caused by strok in restoreThread
- [13] Add comments to each method
- [15] fix misc valgrind issues
- [16] copyFile needs to be able to create directories
- [17] make sure all methods are being error checked, malloc, pthread funcs, etc

## Tests ##
- [t1] Test files without read permission
- [t2] Test backing up large files, csv, and image files
- [t3] Test empty files

# Issues In Progress #

## Tyler ##
[6] Create a thread to restore each file\

## Jacob ##

## Julian ##


# Completed #
- [1] Copy all regular files from the working directory to the .backup directory\
- [2] Compare last modification times of duplicate files before overwriting, and inform the user\
- [3] Create a thread to copy each file
- [4] Perform a restore operation when the -r flag is specified\
- [5] Compare last modification times of duplicate files before restoring, and inform the user
- [7] Modify copy to work recursively on directories