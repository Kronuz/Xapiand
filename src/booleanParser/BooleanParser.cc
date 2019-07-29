/*
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "BooleanParser.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "AndNode.h"
#include "IdNode.h"
#include "MaybeNode.h"
#include "NotNode.h"
#include "OrNode.h"
#include "SyntacticException.h"
#include "XorNode.h"


constexpr auto DEFAULT_OPERATOR = TokenType::Or;


BooleanTree::BooleanTree(std::string_view input_)
	: input(std::make_unique<char[]>(input_.size() + 1))
{
	std::strncpy(input.get(), input_.data(), input_.size());
	input[input_.size()] = '\0';
	lexer = std::make_unique<Lexer>(input.get());
	toRPN();
}


void
BooleanTree::Parse()
{
	root = BuildTree();
	if (!stack_output.empty()) {
		Token token = stack_output.back();
		std::string msj = "'" + token.get_lexeme() + "' not expected";
		throw SyntacticException(msj.c_str());
	}
}


std::unique_ptr<BaseNode>
BooleanTree::BuildTree()
{
	if (stack_output.size() == 1 || stack_output.back().get_type() == TokenType::Id) {
		Token token = stack_output.back();
		stack_output.pop_back();
		std::string id = token.get_lexeme();
		return std::make_unique<IdNode>(id);
	}
	/* Error case */
	if (stack_output.size() == 1 && stack_output.back().get_type() != TokenType::Id) {
		Token token = stack_output.back();
		std::string msj = "'" + token.get_lexeme() + "' not expected";
		throw SyntacticException(msj.c_str());
	} else {
		Token token = stack_output.back();
		stack_output.pop_back();
		switch(token.get_type()) {
			case TokenType::Not:
				return std::make_unique<NotNode>(BuildTree());
			case TokenType::Or:
				return std::make_unique<OrNode>(BuildTree(), BuildTree());
			case TokenType::And:
				return std::make_unique<AndNode>(BuildTree(), BuildTree());
			case TokenType::Maybe:
				return std::make_unique<MaybeNode>(BuildTree(), BuildTree());
			case TokenType::Xor:
				return std::make_unique<XorNode>(BuildTree(), BuildTree());
			default:
				break;
		}
	}

	return nullptr;
}


/*
 * Convert to RPN (Reverse Polish notation)
 * with Dijkstra's Shunting-yard algorithm.
 */
void
BooleanTree::toRPN()
{
	currentToken = lexer->NextToken();
	bool last_token_is_id = false;

	while (currentToken.get_type() != TokenType::EndOfFile) {
		switch (currentToken.get_type()) {
			case TokenType::Id:
				stack_output.push_back(currentToken);
				if (last_token_is_id) {
					while (!stack_operator.empty() && precedence(DEFAULT_OPERATOR) > precedence(stack_operator.back().get_type())) {
						Token token_back = stack_operator.back();
						stack_operator.pop_back();
						stack_output.push_back(token_back);
					}
					Token default_op(DEFAULT_OPERATOR);
					stack_operator.push_back(default_op);
				}
				last_token_is_id = true;
				break;

			case TokenType::LeftParenthesis:
				stack_operator.push_back(currentToken);
				break;

			case TokenType::RightParenthesis:
				while (true) {
					if (!stack_operator.empty()) {
							Token token_back = stack_operator.back();
						if (token_back.get_type() != TokenType::LeftParenthesis) {
							stack_output.push_back(token_back);
							stack_operator.pop_back();
						} else {
							stack_operator.pop_back();
							break;
						}
					} else {
						std::string msj = ") was expected";
						throw SyntacticException(msj.c_str());
					}
				}
				break;

			case TokenType::Not:
			case TokenType::Or:
			case TokenType::And:
			case TokenType::Maybe:
			case TokenType::Xor:
				while (!stack_operator.empty() && precedence(currentToken.get_type()) > precedence(stack_operator.back().get_type())) {
					Token token_back = stack_operator.back();
					stack_operator.pop_back();
					stack_output.push_back(token_back);
				}
				stack_operator.push_back(currentToken);
				last_token_is_id = false;
				break;

			case TokenType::EndOfFile:
				break;
		}
		currentToken = lexer->NextToken();
	}

	while (!stack_operator.empty()) {
		Token tb = stack_operator.back();
		stack_output.push_back(tb);
		stack_operator.pop_back();
	}
}


unsigned
BooleanTree::precedence(TokenType type)
{
	switch (type) {
		case TokenType::Not:
			return 0;
		case TokenType::And:
			return 1;
		case TokenType::Maybe:
			return 2;
		case TokenType::Xor:
			return 3;
		case TokenType::Or:
			return 4;
		default:
			return 5;
	}
}


void
BooleanTree::PrintTree()
{
	postorder(root.get());
}


void
BooleanTree::postorder(BaseNode* p, int indent)
{
	if (p != nullptr) {
		switch (p->getType()) {
			case NodeType::AND:
				if (dynamic_cast<AndNode*>(p)->getLeftNode() != nullptr) {
					postorder(dynamic_cast<AndNode*>(p)->getLeftNode(), indent + 4);
				}
				if (indent != 0) {
					std::cout << std::setw(indent) << ' ';
				}
				std::cout << "AND" << "\n ";
				if (dynamic_cast<AndNode*>(p)->getRightNode() != nullptr) {
					postorder(dynamic_cast<AndNode*>(p)->getRightNode(), indent + 4);
				}
				break;
			case NodeType::MAYBE:
				if (dynamic_cast<MaybeNode*>(p)->getLeftNode() != nullptr) {
					postorder(dynamic_cast<MaybeNode*>(p)->getLeftNode(), indent + 4);
				}
				if (indent != 0) {
					std::cout << std::setw(indent) << ' ';
				}
				std::cout << "MAYBE" << "\n ";
				if (dynamic_cast<AndNode*>(p)->getRightNode() != nullptr) {
					postorder(dynamic_cast<AndNode*>(p)->getRightNode(), indent + 4);
				}
				break;
			case NodeType::OR:
				if (dynamic_cast<OrNode*>(p)->getLeftNode() != nullptr) {
					postorder(dynamic_cast<OrNode*>(p)->getLeftNode(), indent + 4);
				}
				if (indent != 0) {
					std::cout << std::setw(indent) << ' ';
				}
				std::cout << "OR" << "\n ";
				if (dynamic_cast<OrNode*>(p)->getRightNode() != nullptr) {
					postorder(dynamic_cast<OrNode*>(p)->getRightNode(), indent + 4);
				}
				break;
			case NodeType::NOT:
				if (dynamic_cast<NotNode*>(p) != nullptr) {
					std::cout << std::setw(indent) << ' ';
					std::cout << "NOT" << "\n ";
					postorder(dynamic_cast<NotNode*>(p)->getNode(), indent + 4);
				}
				break;
			case NodeType::XOR:
				if (dynamic_cast<XorNode*>(p)->getLeftNode() != nullptr) {
					postorder(dynamic_cast<XorNode*>(p)->getLeftNode(), indent + 4);
				}
				if (indent != 0) {
					std::cout << std::setw(indent) << ' ';
				}
				std::cout << "XOR" << "\n ";
				if (dynamic_cast<XorNode*>(p)->getRightNode() != nullptr) {
					postorder(dynamic_cast<XorNode*>(p)->getRightNode(), indent + 4);
				}
				break;
			case NodeType::ID:
				std::cout << std::setw(indent) << ' ';
				if (dynamic_cast<IdNode*>(p) != nullptr) {
					std::cout << dynamic_cast<IdNode*>(p)->getId() << "\n ";
				}
				break;
		}
	}
}
