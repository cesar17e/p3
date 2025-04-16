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

This test case tests the functionality of the input redirector <. There is pre-written text in the file, and the expected output is:

Testing input direction! Changing directories...
In test folder! Opening inputdummy.txt... syntax: cat<inputdummy.txt
this is text inside a file
and it should all be printed
by the test case. this is expected behavior.


