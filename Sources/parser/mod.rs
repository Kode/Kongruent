mod expression;
mod statement;

use expression::*;
use statement::*;
use super::tokenizer::token::*;

pub fn parse(tokens: Vec<Token>) -> Vec<Statement> {
	let mut state = State { tokens, index: 0 };
	let mut statements = Vec::new();
	loop {
		match state.current() {
			Token::Eof => break,
			_ => statements.push(statement(&mut state)),
		}
	}
	statements
}

struct State {
	tokens: Vec<Token>,
	index: usize,
}

impl State {
	fn current(&self) -> Token {
		let token = self.tokens.get(self.index);
		match token {
			Some(value) => value.clone(),
			None => Token::Eof.clone(),
		}
	}

	fn advance(&mut self) {
		self.index += 1;
	}
}

fn block(state: &mut State) -> BlockStatement {
	match state.current() {
		Token::LeftCurly => state.advance(),
		_ => panic!("Expected an opening curly bracket.")
	}
	let mut statements = Vec::new();
	loop {
		match state.current() {
			Token::RightCurly => {
				state.advance();
				break
			}
			_ => {
				statements.push(statement(state));
			}
		}
	}
	BlockStatement {statements}
}

fn preprocessor(name: String, _state: &mut State) -> PreprocessorStatement {
	let expressions = Vec::new();
	/*loop {
		match state.current() {
			Token::NewLine => {
				state.advance();
				break
			}
			_ => {
				expressions.push(expression(state));
			}
		}
	}*/

	PreprocessorStatement {name, parameters: expressions}
}

enum Modifier {
	In,
	//Out,
}

fn declaration(state: &mut State, modifiers: &mut Vec<Modifier>) -> Statement {
	match state.current() {
		Token::In => {
			state.advance();
			modifiers.push(Modifier::In);
			return declaration(state, modifiers);
		}
		Token::Vec3 => {
			state.advance();
		}
		Token::Vec4 => {
			state.advance();
		}
		Token::Float => {
			state.advance();
		}
		Token::Void => {
			state.advance();
		}
		_ => panic!("Expected a variable declaration.")
	}

	let name: String;
	match state.current() {
		Token::Identifier(value) => {
			state.advance();
			name = value;
		}
		_ => panic!("Expected an identifier.")
	}

	match state.current() {
		Token::Operator(op) => {
			match op {
				Operator::Assign => {
					state.advance();
					let expr = expression(state);
					match state.current() {
						Token::Semicolon => {
							state.advance();
							return Statement::Declaration(DeclarationStatement {name, init: Some(expr)});
						}
						_ => panic!("Expected a semicolon.")
					}
				}
				_ => panic!("Expected an assignment operator.")
			}
		}
		Token::Semicolon => {
			state.advance();
			return Statement::Declaration(DeclarationStatement {name, init: None});
		}
		Token::LeftParen => {
			state.advance();
			match state.current() {
				Token::RightParen => {
					state.advance();
					return Statement::Function(FunctionStatement {parameters: Vec::new(), block: block(state)});
				}
				_ => panic!("Expected right paren.")
			}
		}
		_ => panic!("Expected an assign or a semicolon.")
	}
}

fn statement(state: &mut State) -> Statement {
	match state.current() {
		Token::Attribute(value) => {
			state.advance();
			Statement::PreprocessorDirective(preprocessor(value, state))
		}
		Token::If => {
			state.advance();
			match state.current() {
				Token::LeftParen => state.advance(),
				_ => panic!("Expected an opening bracket."),
			}
			let test = expression(state);
			match state.current() {
				Token::RightParen => state.advance(),
				_ => panic!("Expected a closing bracket."),
			}
			let block = statement(state);
			Statement::If(IfStatement {test, block: Box::new(block)})
		}
		Token::LeftCurly => {
			Statement::Block(block(state))
		}
		Token::In => {
			declaration(state, &mut Vec::new())
		}
		Token::Float => {
			declaration(state, &mut Vec::new())
		}
		Token::Vec3 => {
			declaration(state, &mut Vec::new())
		}
		Token::Vec4 => {
			declaration(state, &mut Vec::new())
		}
		Token::Void => {
			declaration(state, &mut Vec::new())
		}
		Token::Struct => {
			parse_struct(state)
		}
		_ => {
			let expr = expression(state);
			match state.current() {
				Token::Semicolon => {
					state.advance();
					Statement::Expression(expr)
				}
				_ => panic!("Expected a semicolon."),
			}
		}
	}
}

fn expression(state: &mut State) -> Expression {
	assign(state)
}

fn assign(state: &mut State) -> Expression {
	let mut expr = logical(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Assign => {
						state.advance();
						let right = logical(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn logical(state: &mut State) -> Expression {
	let mut expr = equality(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Or | Operator::And => {
						state.advance();
						let right = equality(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn equality(state: &mut State) -> Expression {
	let mut expr = comparison(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Equals | Operator::NotEquals => {
						state.advance();
						let right = comparison(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn comparison(state: &mut State) -> Expression {
	let mut expr = addition(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Greater | Operator::GreaterEqual | Operator::Less | Operator::LessEqual => {
						state.advance();
						let right = addition(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn addition(state: &mut State) -> Expression {
	let mut expr = multiplication(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Minus | Operator::Plus => {
						state.advance();
						let right = multiplication(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn multiplication(state: &mut State) -> Expression {
	let mut expr = unary(state);
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Div | Operator::Multiply | Operator::Mod => {
						state.advance();
						let right = unary(state);
						expr = Expression::Binary(ExprBinary {left: Box::new(expr), op: op, right: Box::new(right)});
					}
					_ => break
				}
			}
			_ => break
		}
	}
	expr
}

fn unary(state: &mut State) -> Expression {
	loop {
		match state.current() {
			Token::Operator(op) => {
				match op {
					Operator::Not | Operator::Minus => {
						state.advance();
						let right = unary(state);
						return Expression::Unary(ExprUnary {op: op, right: Box::new(right)});				
					}
					_ => break
				}
			}
			_ => break
		}
	}
	primary(state)
}

fn primary(state: &mut State) -> Expression {
	match state.current() {
		Token::Boolean(value) => {
			state.advance();
			Expression::Boolean(value)
		}
		Token::Number(value) => {
			state.advance();
			Expression::Number(value)
		}
		Token::Stringy(value) => {
			state.advance();
			Expression::Stringy(value)
		}
		Token::Identifier(value) => {
			state.advance();
			match state.current() {
				Token::LeftParen => Expression::Call(parse_call(state, Expression::Variable(value))),
				Token::Colon => {
					state.advance();
					match state.current() {
						Token::Identifier(value2) => {
							state.advance();
							let member = Expression::Member(MemberExpression {value1: value, value2: value2});
							match state.current() {
								Token::LeftParen => Expression::Call(parse_call(state, member)),
								_ => member,
							}
						}
						_ => panic!("Expected an identifier.")
					}
				}
				_ => Expression::Variable(value)
			}			
		}
		Token::LeftParen => {
			state.advance();
			let expr = expression(state);
			match state.current() {
				Token::RightParen => (),
				_ => panic!("Expected a closing bracket.")
			}
			Expression::Grouping(Box::new(expr))
		}
		Token::Vec4 => {
			state.advance();
			match state.current() {
				Token::LeftParen => (),
				_ => panic!("Expected an opening bracket.")
			}
			state.advance();

			let mut expressions = Vec::new();
			loop {
				match state.current() {
					Token::RightParen => break,
					_ => {
						expressions.push(expression(state));
					}
				}
				state.advance();
				match state.current() {
					Token::Comma => (),
					Token::RightParen => break,
					_ => {
						expressions.push(expression(state));
					}
				}
				state.advance();
			}
			state.advance();
			Expression::Constructor(ConstructorExpression {parameters: expressions})
		}
		_ => {
			panic!("Unexpected token: {:?}", state.current())
		}
	}
}

fn parse_call(state: &mut State, func: Expression) -> CallStatement {
	match state.current() {
		Token::LeftParen => {
			state.advance();
			match state.current() {
				Token::RightParen => {
					state.advance();
					CallStatement {func: Box::new(func), parameters: vec![]}
				}
				_ => {
					let expr = expression(state);
					match state.current() {
						Token::RightParen => {
							state.advance();
							CallStatement {func: Box::new(func), parameters: vec![expr]}
						}
						_ => panic!("Expected a closing bracket.")
					}
				}
			}
		}
		_ => panic!("Fascinating")
	}
}

fn parse_struct(state: &mut State) -> Statement {
	let name: String;
	let member_name: String;
	let type_name: String;

	state.advance();
	match state.current() {
		Token::Identifier(value) => {
			name = value;
			state.advance();
			match state.current() {
				Token::LeftCurly => {
					state.advance();
					match state.current() {
						Token::Identifier(value) => {
							member_name = value;
							state.advance();
							match state.current() {
								Token::Colon => {
									state.advance();
									match state.current() {
										Token::Identifier(value) => {
											type_name = value;
											state.advance();
											match state.current() {
												Token::Semicolon => {
													state.advance();
													match state.current() {
														Token::RightCurly => {
															state.advance();
														}
														_ => panic!("Expected a closing curly bracket.")
													}
												}
												_ => panic!("Expected a semicolon.")
											}
										}
										_ => panic!("Expected an identifier.")
									}
								}
								_ => panic!("Expected a colon.")
							}
						}
						_ => panic!("Expected an identifier.")
					}
				}
				_ => panic!("Expected an opening curly bracket.")
			}
		}
		_ => panic!("Expected an identifier.")
	}
	let member = Member {name: member_name, member_type: type_name};
	Statement::Struct(StructStatement {attribute: String::new(), name, members: vec![member]})
}
