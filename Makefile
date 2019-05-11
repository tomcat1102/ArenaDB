# Top makefile. 

# Default 'make' goal is 'all', since there is not rule for 'all', it 
# will carry out the recipe in .DEFAULT which, in this case, is 
# 'cd src && make all'. And the corresponding recipe for target 'all' in
# src/makefile would be executed.

# If 'make' is executed with a explicit goal, say 'clean', since there is 
# no goal 'clean' in this file, the execution of 'make clean' will become
# 'cd src && make clean' as in .DEFAULT and corresponding recipe for target
# 'clean' in src/makefile would be executed.

default: all

.DEFAULT:
	cd src && $(MAKE) $@


