# **************************************************************************** #
# COMPILE TARGETS #
# **************************************************************************** #

# Executable names
KRPSIM 			:= krpsim
KRPSIM_VERIF	:= krpsim_verif

# **************************************************************************** #
#                                 INGREDIENTS                                  #
# **************************************************************************** #

COMMON_SRC 			:= src/parsing.cpp src/helper.cpp
KRPSIM_SRC 			:= src/krpsim.cpp src/genetic_algo.cpp $(COMMON_SRC)
KRPSIM_VERIF_SRC	:= src/krpsim_verif.cpp $(COMMON_SRC)

# Object files (stored in .build/ keeping tree structure)
KRPSIM_OBJS 		:= $(KRPSIM_SRC:%.cpp=.build/%.o)
KRPSIM_VERIF_OBJS	:= $(KRPSIM_VERIF_SRC:%.cpp=.build/%.o)
ALL_OBJS 			:= $(KRPSIM_OBJS) $(KRPSIM_VERIF_OBJS)
DEPS			 	:= $(ALL_OBJS:%.o=%.d)

# **************************************************************************** #
#                                TOOLS & FLAGS                                 #
# **************************************************************************** #

CXX				:=	c++
CXXFLAGS		:=	-Wall -Wextra -Werror -std=c++17 -g
CPPFLAGS		:=	-MP -MMD -Iinclude
LDFLAGS			:=

MAKEFLAGS		+= --silent --no-print-directory

# **************************************************************************** #
#                                   RECIPES                                    #
# **************************************************************************** #

all: header $(KRPSIM) $(KRPSIM_VERIF)

$(KRPSIM): $(KRPSIM_OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@
	@printf "%b" "$(BLUE)CREATED $(CYAN)$@\n"


$(KRPSIM_VERIF): $(KRPSIM_VERIF_OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@
	@printf "%b" "$(BLUE)CREATED $(CYAN)$@\n"

.build/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $(CPPFLAGS) $< -o $@
	@printf "%b" "$(BLUE)CREATED $(CYAN)$@\n"

-include $(DEPS)

clean:
	rm -rf .build

fclean: clean
	rm -rf $(KRPSIM) $(KRPSIM_VERIF) trees.txt

re:
	$(MAKE) fclean
	$(MAKE) all

# **************************************************************************** #
#                                    STYLE                                     #
# **************************************************************************** #

GREEN			:= \033[0;32m
YELLOW			:= \033[0;33m
BLUE			:= \033[0;34m
CYAN			:= \033[0;36m
OFF				:= \033[m

header:
	@printf "%b" "$(GREEN)"
	@echo " _  __   ____    ____     ____   ___    __  __"
	@echo "| |/ /  |  _ \\\\  |  _ \\\\  / ___|  |_ _|  |  \\\\/  |"
	@echo "| ' /   | |_) | | |_) | \\\\___ \\\\   | |   | |\\\\/| |"
	@echo "|  <    |  _ <  |  __/   ___) |  | |   | |  | |"
	@echo "|_|\\\\_\\\\  |_| \\\\_\\\\ |_|     |____/  |___|  |_|  |_|"
	@echo
	@printf "%b" "$(CYAN)CXX:      $(YELLOW)$(CXX)\n"
	@printf "%b" "$(CYAN)CXXFlags: $(YELLOW)$(CXXFLAGS)\n"
	@printf "%b" "$(OFF)"
	@echo

# **************************************************************************** #
#                                   SPECIAL                                    #
# **************************************************************************** #

.PHONY: all clean fclean re
.DELETE_ON_ERROR: