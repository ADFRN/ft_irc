/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: afournie <afournie@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/26 16:14:26 by afournie          #+#    #+#             */
/*   Updated: 2026/10/26 16:29:18 by afournie         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client() {}

Client::~Client() {}

int Client::getFd() { return this->_fd; }

void Client::setFd(int fd) { this->_fd = fd; }

void Client::setIpAdd(std::string ipAdd) { this->_ipAdd = ipAdd; }
