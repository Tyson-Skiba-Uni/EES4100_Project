#!/bin/bash                       
#Standard for a Shell Script Says its Designed for Bash
#Must be made Executable "chmod u+x ./bootstrap.sh"

autoreconf --install              #Run This Command
./configure
if make; then
	printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
	COLUMNS=$(tput cols) 
	bold=$(tput bold )
	title=" ${bold}EES4100: Operating Systems and Networking" 
	printf "%*s\n" $(((${#title}+$COLUMNS)/2)) "$title"
	printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
	normal=$(tput sgr0)
	title="${normal}Bridge Has Been Localy Built." 
	printf "%*s\n" $(((${#title}+$COLUMNS)/2)) "$title"
	title="To Install Run 'make install' as root" 
	printf "%*s\n" $(((${#title}+$COLUMNS)/2)) "$title"
	title="Alternatively, You May Now Execute the Program by typing './src/bridge'" 
	printf "%*s\n" $(((${#title}+$COLUMNS)/2)) "$title"
	printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
else
    echo "Command failed"
fi
