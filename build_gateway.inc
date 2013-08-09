# 
# This file includes all dynamically changing build-related
# variables. 
#
# Modify to match with your needs. Do not commit any private
# changes to this file!
#

#
# Set debug flags
#
DEBUG         := Y
# DEBUGGER      := 
# DEBUGGER_PATH :=
# DEBUGGER_BIN  :=
#
# Set build env
#
UNIX := Y

# 
# Set path for root directory, that is, path to directory where
# makefile.inc and build_gateway.inc are located.
# ROOT_PATH is used in makefile.
#
ROOT_PATH := /home/jan/skysql/maxscale

# MARIADB_SRC_PATH may be defined either as an environment variable or
# specifically here
ifndef $(MARIADB_SRC_PATH)
       MARIADB_SRC_PATH := /home/jan/mysql/5.5
endif