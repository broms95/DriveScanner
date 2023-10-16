# DriveScanner
Interesting little project to scan a drive as fast as possible for sensitive data

# Problem
Design drive scanning application to scan 100,000 files in 5 minutes.  Assume any type of file could be encountered. 

Assumptions from customer requirements
1.	Would like the solution as portable as possible.
2.	OK with some false positives, but as few as possible.
3.	Would like to add support for different file types over time.
4.	Would like to add support for different patterns to be found over time
5.	Would like to support compressed files.
6.	Attempt to make solution in 2 hours (took me a little longer...)

I also assume a few more things:
1.	The solution should be a single application running on a single computer.  Access to files is for the most part local.  Will not optimize for tortuous search patterns (99% of the files are 1 byte, and 1 file is 100GB, or the filesystem hierarchy is 100,000 levels deep, etc.)
2.	This is a modern computer that can run multiple threads.  Memory is reasonable, say 1 GB available for this process.
3.	The report can simply be output from a console application.  We could send results over a socket to some larger scanning effort, but that’s simply too big of a problem to show a good solution in a few hours of work.

Given performance is a priority, I choose to use C++ and standard C++ libraries for scanning the filesystem.  

# Design of application
To keep the solution simple but fast, we will break up the problem into 2 passes.
1.	Scan all the directories and files within to create a list of items to scan
2.	Create a thread pool to open and scan each file individually, report matched files as they are encountered.

Clearly the work queue will need to be protected.  A simple mutex will suffice.  The output (std::cout) will also need to be protected so you don’t interrupt writing a line with another line.  A mutex will work for this too (not the same as work queue).

Given this application could easily be IO bound (vs CPU bound from the actual string search), the optimal number of threads could be more than the number of cores on the machine.

# File Type Plugin Design
When each individual file is scanned, we must be determine what filetype this is.  The simplest way to do this is by examining the extension (which is by no means foolproof).  For each extension we could register a separate handler that is used for parsing the file. 

A special class of file types would be a compressed.  These would need their own plugin to open, then hand off each file within to another plugin.  Should be done in memory to be efficient.  The most generic solution would make this process recursive.

The input to the plugin is a block of data called repeatly, the output would be another block of data, either in text (and ready to search) or another format (would would go input a different plugin in the system).  

For example a .zip file would cause the zip file plugin to be created and then handed its first 4K of memory, this in turn could output a block of a pdf file.  This in turn could output a block of searchable text.

Once the text is encountered, the text is searched (more below)

# Searching a single text file
To be fast, searching would be done on a block by block basis (say 4 or 8K wide).  The search should be performed on just this block (this assumes the search pattern is small like the customer used as an example 9 character SSN or 16 character CC number).  We would need to handle cases where the match pattern overlaps the IO block, but that’s much faster than loading the entire file.  It also gives us the opportunity to exit early

# Pattern Matching Plugin Design
Different patterns will need to be searched--which might expand over time.  The simplest (but perhaps not the fastest) would be to use a regular expression evaluator.  Another option is to craft code for each pattern we are interested in, and call this code as each block is examined.

For example the social security algorithm could look for substrings with 9 digits in a row or in the format XXX-XX-XXXX.

The credit card algorithm could look for substrings with either 16 digits in a row, or 4 sets of 4 numbers. Interestingly CC numbers are hashed and a quick evaluation could be made on the spot.  For example VISA cards (they all start with 4).  A robust CC scanner is much more than looking for 16 digits in a row…see https://www.freeformatter.com/credit-card-number-generator-validator.html

Later on, we could search for drivers licenses or passwords or login credentials.

# Intermediate search representation
To make life easier, and faster, we can convert the text string to a simpler form
Convert all digits to character “0”
Keep spaces and dashes alone
Convert all other characters to a dot (.)  These are considered whitespace.

For example if a text file read:
“Please buy 3 apples with my CC number 1234 5678 1234 5678” it would be converted to 
“...... ... 0 ...... .... .. .. ...... 0000 0000 0000 0000”

Then searching for a CC number would just be matching “0000000000000000” or “0000 0000 0000 0000”
And search for SSN number would just be matching “000000000” or “000-00-0000”

What I can do in a reasonable amount of time
1.	Scan first 100,000 directories
2.	Create a thread pool and start chewing through the work queue.  
3.	For each file assume latin1 raw text file and open them and search for SSN numbers and CC cards.
a.	Open files in blocks of 4K each 
b.	Convert to intermediate form
c.	Search for SSN
d.	Search for CC

# Results
Crafting a good heiristic to catch SSN or CC numbers without many false positives looks tricky.  Especially if the numbers are not formatted like 123-45-6789.  It is easy to pickup random 9 digit numbers in gigabytes of raw text data on a disk.

One interesting area to look into is using the Luhn formula to do a first-pass validation on the credit card numbers.  It looks very fast and would reduce false positives by 10 (since it is using the last digit as a quasi-checksum).  Using a table of known CC number prefixes would also help.  

Even better would be looking at the text and formatting around the string to get better context.  Of course this would take time to do right, require test sets.  Almost feels like you could use machine learning to solve for this problem vs trying to handcraft the world's best CC extractor.

Here's running a scan on 78K files of various sizes in my development directory.  
```
Threads:                30
Directories scanned:    16208
Files scanned:          79525
Bytes scanned           26.4679 GB
Time for phase 1:       0.837 sec
Time for phase 2:       2.147 sec
Total scanning time:    2.984 sec
Bandwidth (per thread): 0.295664 GB/sec
Bandwidth (aggregate):  8.86993 GB/sec
```

Also tried different number of threads.  I have a fairly old workstation with only 4 cores, but it has a very fast SSD (Samsung 9700 something...).  
```
Threads    GB/s per thread    GB/s total
30         0.29               8.86
20         0.46               9.39
15         0.56               8.54
10         0.82               8.25
5          1.45               7.25
4          1.64               6.56
3          1.88               5.64
2          2.11               4.23
1          2.37               2.37
```
Using 5-20 threads seems like the sweet spot -- past that the total bandwidth processed goes down.  Would be good to be conservative here because each thread add overhead, memory, etc.

A few more bits of low hanging fruit:
1. Starting phase 2 right after phase 1 starts could save real time
2. Making phase 1 multithreaded instead of marching through a directory tree in a single thread might help too.
