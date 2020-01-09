To run httpserver the command is -a alias_filename ./httpserver -l filename -N # ip/hostname port# ...

Note: 
- Netcat depending on the requests sent tends to hang for longer then desired, but if given
  time it will complete all tasks properly. It is still unknown what the issue is.
- If a logging flag/name is passed in there is noticable slowdown in the code when running requests




