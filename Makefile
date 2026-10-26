# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: afournie <afournie@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/10/26 15:57:40 by afournie          #+#    #+#              #
#    Updated: 2026/10/26 15:58:32 by afournie         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

SRC_DIR		=	.
OBJ_DIR		=	obj

SRC			=

INC			=	

OBJ			=	$(SRC:%.cpp=$(OBJ_DIR)/%.o)

CXX			=	c++
CFLAGS		=	-std=c++98 -Wall -Wextra -Werror

RM			=	rm -rf

NAME		=	ircserv

all: $(NAME)

$(NAME): $(OBJ) $(INC)
	$(CXX) $(CFLAGS) $(OBJ) -o $(NAME)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
