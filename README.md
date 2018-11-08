sws -- a simple web server
  
MAIN FUNCTIONS USED
--------------------------------------
* inet_pton(3) - convert the user provided address from printable form to network usable form
* strtol(3) - used to convert the port number from a user-provided string to an int while also checking the validity of the provided port


ISSUES
--------------------------------------
  
* While parsing the provided address we have to check for the following
  - the length of the provided address should not exceed INET_ADDRSTRLEN (IPv4) and INET6_ADDRSTRLEN (IPv6)
  - the provided address should be convertible to network format. For example, these are not valid (192. , 192.1.455.256, 235:2352:,localhost
    etc.)
