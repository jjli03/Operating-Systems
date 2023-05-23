# Operating-Systems
## Project 4:
This projects creates two instances of an HTTP server which is able to comunicate with a browser which acts as an HTTP client. Communication is done through the set up of TCP sockets, bind(), listen(), and headers and data is sent through accept(), read(), and write() calls. The HTTP server utilizes a read function to obtain the name of the file requested, and a write function to post a response to the client in the correct HTTP format e.g. 404 error for file not found or 200 for success.

In part 2 of the project, multithreading and sychronization are utilized to handle more than one request at a time. A queue is initialized so that a maximum of five requests are handled at a time and condition statements are used to block addition and subtraction from such queue when it is full with five or empty with zero. The resource directory is server_files which has different file types like pdfs, htmls, txts... to demonstrate the utility of the server. 

Testing: To utilize the web browser, run one of the programs with ./http_server server_files <port_number> e.g. 8000. Then, open up a browser with a local host http://localhost:8000/<file_name> e.g. index.html. 
