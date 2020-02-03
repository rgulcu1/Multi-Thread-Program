 #Multi Thread Program 
 
 In this project ,  program that uses threads and synchronization to read from a file, modify its contents and write to the same file.Program with four types of threads, structured like:
 
 ![Figure](/image.png)
 
*The Read Thread will read from a file line by line and store every line in and array.

*The Upper Thread will read from the array, convert the string to uppercase letters and write back
to the same index of the array.

*The Replace Thread will read from the array, replace all spaces with underscore character and
write back to the same index of the array.

*The Write Thread will read from the array and store it to the text file, each line in the corrent
place.
