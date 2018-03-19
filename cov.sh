#!/bin/sh

NORM="\033[0m"
BOLD="\033[1m"

make clean

printf "${BOLD}Build dis for coverage analysis${NORM}\n"
make dis.cov

printf "${BOLD}non-standard invocations${NORM}\n"
./dis
./dis -m sane runtime/ka655x.bin -o runtime/ka655x-test
./dis -m sane -o runtime/ka655x-test runtime/ka655x.bin
./dis -m sane -o runtime/ka655x-test -o
./dis -m sane -o
./dis -m fnok
./dis -m
./dis -m sane -m vax runtime/ka655x.bin

printf "${BOLD}coverage w/ VAX syntax${NORM}\n"
./dis -m vax runtime/ka655x.bin
./dis -m vax runtime/aclock-vax-netbsd2
./dis -m vax runtime/aclock-vax-openbsd3
./dis -m vax runtime/aclock-vax-ultrix4
./dis -m vax runtime/aclock-vax-vms6.exe

printf "${BOLD}coverage w/ sane syntax${NORM}\n"
./dis -m sane runtime/ka655x.bin
./dis -m sane runtime/aclock-vax-netbsd2
./dis -m sane runtime/aclock-vax-openbsd3
./dis -m sane runtime/aclock-vax-ultrix4
./dis -m sane runtime/aclock-vax-vms6.exe

printf "${BOLD}Generate HTML report${NORM}\n"
make cov


