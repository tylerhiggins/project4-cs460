# TODO #

- [7] Modify program to work recursively on directories
- [8] Test backing up large files, csv, and image files
- [9] Make sure the restoreThread closes the FP
- [10] Make sure the recursiveRestore closes the backupDir
- [11] use a mutex on printf("Copied %d files (%d bytes)\n",successfulFiles, totalBytes); since it is shared
- [12] Fix memory leaks caused by strok in restoreThread

## Tests ##

# Issues In Progress #

## Tyler ##
[6] Create a thread to restore each file\

## Jacob ##

## Julian ##


# Completed #
[1] Copy all regular files from the working directory to the .backup directory\
[2] Compare last modification times of duplicate files before overwriting, and inform the user\
[3] Create a thread to copy each file
[4] Perform a restore operation when the -r flag is specified\
[5] Compare last modification times of duplicate files before restoring, and inform the user
