#!/bin/bash                       
#Standard for a Shell Script Says its Designed for Bash
#Must be made Executable "chmod u+x ./bootstrap.sh"

autoreconf --install              #Run This Command
./configure
make
