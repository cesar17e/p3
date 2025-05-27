Cesar Estrada (ce223), Brandon Son (bjs334)
CS214
Assignment 3 - My Shell


PROGRAM DESIGN AND OVERVIEW
===========================

This program is designed to replicate the performance and interactions of a shell along with adding some special functionality for specific keywords (and, or, < >, |, etc)

The program first determines if it is running in interactive mode or if it reading arguments from a file. 
An ArrayList is then initalised to store tokens from commands. Additionally, if the program is running in interactive mode, it will also display a prompt in order for the user to input commands.

Whether the command is inputted from a file or from the terminal and depending on the types of command that is inputted, the program handles the lines differently. For example, if a pipe operator (|) is present, the shell creates multiple command structures that are connected hwich enables it to fork child processes where the output of a command is allocated to a pipe and another procooess reads from it, functionally chaining these two commands together (will be explained in further detail below)

PARSING AND READING LINES
=========================

The process_lines function reads data either from standard input or a file into a buffer. It then iterates through the buffer, scanning for any new line characters. When one is encountered, the full line is passed to the tokenizeCommand function, which uses helper function insideAWord() to determine when each token starts and ends. In addition, it checks for additional characters that may affect how the command is read, i.e the # to determine that the text is a comment and the text should not be run as a command.

Each token is then appended to an ArrayList to store all the tokens for the current command line. 

The command structure is then built by interpreting each entry of the ArrayList. The program looks for special symbols such as < and >, and stores attributes such as inputFile and outputFile into the command structure 

|| PIPING ||
============

When encountering a | symbol, the program sets the pipePresent flag and then creates a new command structure, as well as linking the subsequent command through the built in next pointer. The linked list thereby represents the different stages of the pipeline (each node corresponding to a command in the pipeline), which is how the output of one process is able to be sent to a pipe, which can be read as the input to another process, essentially linking these two processes together.

ADDITIONAL SCANNING
===================

The function also looks for the conditional operators "and" and "or"; when one of these conditions are found, it is added to the command structure as a condition attribute. 

Finally, the parser also checks for tokens containing wildcard (*) charcters; if one is encountered, the program will call the expandWildCard function to include matching filenames containing the specifications of the query to the command arguments. If no matches are found, the token is added to the command's argument list. 

After we process all the tokens, we call finalizeArgs to attach a null poitner to the end of the arguments array. 

EXECUTING COMMANDS
===================

When processing and parsing the line is complete, we execute the command calling the executeCommand function (shocking, I know...). First, the function checks if the command structure has a conditional operator such as "and" or "or"; if the command does have one of those two words, the exit status of the previous command is referenced (which is tracked by a global variable) in order to determine whether the current command runs. The and command only runs after a success and the or command only runs after a failure.

For single, non-pipeline commands, the shell checks to see if the command is a built-in command, which are processes that can be executed within the same process. In the case of redirection, we store a duplicate of the standard input or output so we can point to the files specified by the redirection signs. After executing the command, it then restores the original data.

For non-built in processes (processes that cannot be executed within the same process), we fork a new child process. 

Before calling execv to replace the child process’s image, the shell resolves the executable path. If the executable is foudn and accessible, the child process uses execv to execute it. If errors occur during redirection or command execution, proper error messages are printed, and the process exits accordingly.

In the case of pipelines, two child processes are forked, one for the left and one for the right. The left child has its standard output redirected to the write end of the pipe, and the right child has standard input redirected to the read end of the pipe. Both children will execute the commands (either built in or using execv), while the parent process closes the pipe's file descriptors and waits for both child procoesses to complete.

    TEST CASES
========================

Test cases can be found in ./testfolder. Each repository stores its own test case


inputdirectiontest
==================

Run by doing ./mysh ./testfolder/inputdirectiontest/inputdirectiontest.txt

This test case tests the functionality of the input redirector <. There is pre-written text in the file, and the expected output is:

Testing input direction! Changing directories...
In test folder! Opening inputdummy.txt... syntax: cat<inputdummy.txt
this is text inside a file
and it should all be printed
by the test case. this is expected behavior.
File inputdummy.txt read. Test Complete!


builtInFunctionTest
===================
Run by doing ./mysh ./testfolder/builtInFunctionTest/builtInFunctionTest.txt

This test cases tests the functionality of cd .., ls, cat, wildcard, echo, and the output redirection operator >. This test case puts all the text files in the working directory into output.txt, changes directory to the parent directory, prints out the output file contents, and prints out the current directory. 

Expected output:

Printing all txt files
cat: output.txt: No such file or directory
We are in directory:
/common/home/[netID]
Test Complete!

testPiping
============
Run by doing ./mysh ./testfolder/testPiping/testingpipe.txt

This test case tests the functionality of piping within the shell. It evaluates three different commands that involve pipes. The first command, ls | grep .c, should list only those files in the current working directory whose names include the substring “.c”. The second command, cat doesNotExist.txt | grep foo, is expected to produce an error because the file “doesNotExist.txt” does not exist—resulting in an error message from cat (for example, “cat: doesNotExist.txt: No such file or directory”) with no output forwarded to grep. The third command, ls | grep txt, should display only the files with names containing “txt” in the working directory.

Expected output:
arraylist.c
builtInCommands.c
mysh.c
cat: doesNotExist.txt: No such file or directory
authors.txt
foo.txt
golem.txt
output.txt
test.txt
Test Complete!

wildcardtestcase 
================
Run by doing ./mysh ./testfolder/wildcardtestcase/wildcard.txt

This test case tests the robustness of the shell's ability to expand wildcards properly. This test folder has several files with various names in it. The program is tasked to put the results of ls a*a into output4.txt. TThis should search for any file that begins and ends with a.
It then prints out the contents of output4.txt, and the expected output.

Expected output:

Searching through all files that start & end with a
Using a*a, inputting it into ls executable (syntax: a*a<ls )
In the wildcardtestcase directory....
addenda 
alaska
alpha
amanda
anacona
anna
attenda 

EXPECTED output: addenda, alaska, alpha, amanda, anacona, anna, attenda 
Test Complete!


andOrTestCases
=============

This series of tests are used to test the functionality of the conditions. More specifically, these objectives must be met:
If a command has and or or, it must not be run if it is the first command of the shell.
If and is observed in a command, it must only run if the previous command ran.
If or is observed in a command, it must only run if the previous command did not run.

andortestcase.txt
=================

Running: ./mysh ./testfolder/andOrTestCase/andortestcase.txt

This test starts out with true, ensuring that the first process succeeds. It then has two and commands and one or command that follows. The two and commands should run, while the or command should not. 

Expected output:
arraylist.c  arraylist.o  builtInCommands.c  builtInCommands.o  golem.txt  mysh    mysh.o      README.md   testmolder
arraylist.h  authors.txt  builtInCommands.h  foo.txt            Makefile   mysh.c  output.txt  testfolder  test.txt
"This should work! Printing this out is expected!"
Skipping command due to 'or' condition (prevExitStatus = 0).
Test Complete!


beginningAndOr.txt
==================

Running: ./mysh ./testfolder/andOrTestCases/beginningAndOr.txt

This test case should test the edge case where and or or files are used as the first commands. More specifically, these commands should not run, and there should be an error message for each command. Furthermore, the exit status should not be updated, so they are neither considered successful or failed processes. And should only run at the first successful process.

Expected output:
Error: 'and' or 'or' command provided when this is the first command run
Error: 'and' or 'or' command provided when this is the first command run
Error: 'and' or 'or' command provided when this is the first command run
Error: 'and' or 'or' command provided when this is the first command run
arraylist.c  arraylist.o  builtInCommands.c  builtInCommands.o  golem.txt  mysh    mysh.o      README.md   testmolder
arraylist.h  authors.txt  builtInCommands.h  foo.txt            Makefile   mysh.c  output.txt  testfolder  test.txt
/common/home/bjs334/p3
Skipping command due to 'or' condition (prevExitStatus = 0).
Test Complete!

This test works as intended

meow.txt
========

This test case is a very simple test case. It starts off with a true, ensuring that the exit status is set to successful. It should then only print out one line:  "This should print out when first command is run! This is expected!" before printing out Test Complete! There is an or print line that should not be printed. 

Expected output:  "This should print out when first command is run! This is expected!"
                  Skipping command due to 'or' condition (prevExitStatus = 0).
                   Test Complete!


Testing Hashtags 
================
In the project it states that hashtags should have no affect on the programas it its just a comment, the program should ignore anything after a # untill a new line character
The test case below checks to see if this is true for my program

ls #ls --> this code should not be able to run
pwd 
#and ls --> again nothing should occur after this
#Nothing should be happening here 
#Again

As expected, the case ran well and only ran ls and pwd within my program ignoring all hashtags!
Test was a success


testExec
========
This test file is designed to verify that our shell handles the execution of theexternal commands. The file contains a list of commands along with comments that indicate the expected behavior. 

echo Hello! #Should repeat hello
ls # An exec that lists current directories files
true #true an exec that exits with 0
false #false is an executable that exits with status 1.
which ls # Which command, should  print out the location of a given command
pwd #Prints current working directory

As expected the program works fine and does the according instructions along the commands given to the program
Test was a success



