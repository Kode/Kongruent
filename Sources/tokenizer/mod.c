Token *tokenize(const char *source) {
	Token *tokens;
	Mode mode = Mode::Select;
	const char *buffer;

	char *it = source.chars();
	State state(it);

	for (;;) {
		match state.next {
			None = > {
				match mode {
					Mode::Identifier = > {
						push_identifier(&mut tokens, &buffer);
					}
					Mode::Attribute = > {
						push_attribute(&mut tokens, &buffer);
					}
					Mode::Number = > {
						tokens.push(Token::Number(parse_number(&buffer)));
					}
					Mode::Select | Mode::LineComment = > (), Mode::Stringy = > panic !("Unclosed string"),
					               Mode::Operator = > panic !("File ends with an operator"), Mode::Comment = > panic !("Unclosed comment"),
				}
				break tokens;
			}
			Some(ch) = > {
				match mode {
					Mode::Select = > {
						if ch
							== '/' {
								if let
									Some(next_ch) = state.next_next {
										mode = match next_ch {
											'/' = > Mode::LineComment, '*' = > Mode::Comment, _ = > {
												buffer = ch.to_string();
												Mode::Operator
											}
										}
									}
							}
						else if ch
							== '#' {
								state.advance();
								match state.next {
									Some(ch) = > {
										if ch
											!= '[' {
												panic !("Expected [.");
											}
									}
									None = > {
										panic !("Expected [.");
									}
								}

								mode = Mode::Attribute;
								buffer = String::new ();
							}
						else if is_num (ch) {
							mode = Mode::Number;
							buffer = ch.to_string();
						}
						else if is_op (ch) {
							mode = Mode::Operator;
							buffer = ch.to_string();
						}
						else if is_whitespace (ch) {
						}
						else if ch
							== '(' {
								tokens.push(Token::LeftParen);
							}
						else if ch
							== ')' {
								tokens.push(Token::RightParen);
							}
						else if ch
							== '{' {
								tokens.push(Token::LeftCurly);
							}
						else if ch
							== '}' {
								tokens.push(Token::RightCurly);
							}
						else if ch
							== ';' {
								tokens.push(Token::Semicolon);
							}
						else if ch
							== '.' {
								tokens.push(Token::Dot);
							}
						else if ch
							== ':' {
								tokens.push(Token::Colon);
							}
						else if ch
							== ',' {
								tokens.push(Token::Comma);
							}
						else if ch
							== '"' || ch == '\'' {
								mode = Mode::Stringy;
								buffer = String::new ();
							}
						else {
							mode = Mode::Identifier;
							buffer = ch.to_string();
						}
						state.advance();
					}
					Mode::LineComment = > {
						if ch
							== '\n' {
								mode = Mode::Select;
							}
						state.advance();
					}
					Mode::Comment = > {
						if ch
							== '*' {
								if let
									Some(ch) = state.next_next {
										if ch
											== '/' {
												mode = Mode::Select;
												state.advance();
											}
									}
							}
						state.advance();
					}
					Mode::Number = > {
						if is_num (ch) || ch == '.' {
								buffer.push(ch);
								state.advance();
							}
						else {
							tokens.push(Token::Number(parse_number(&buffer)));
							mode = Mode::Select;
						}
					}
					Mode::Operator = > {
						let mut long_op = buffer.clone();
						long_op.push(ch);
						if long_op
							== "==" || long_op == "!=" || long_op == "<=" || long_op == ">=" || long_op == "||" || long_op == "&&" || long_op == "->" {
								buffer.push(ch);
								state.advance();
							}
						else {
							match &buffer[..]{
							    "==" = > tokens.push(Token::Operator(Operator::Equals)),
							    "!=" = > tokens.push(Token::Operator(Operator::NotEquals)),
							    ">" = > tokens.push(Token::Operator(Operator::Greater)),
							    ">=" = > tokens.push(Token::Operator(Operator::GreaterEqual)),
							    "<" = > tokens.push(Token::Operator(Operator::Less)),
							    "<=" = > tokens.push(Token::Operator(Operator::LessEqual)),
							    "-" = > tokens.push(Token::Operator(Operator::Minus)),
							    "+" = > tokens.push(Token::Operator(Operator::Plus)),
							    "/" = > tokens.push(Token::Operator(Operator::Div)),
							    "*" = > tokens.push(Token::Operator(Operator::Multiply)),
							    "!" = > tokens.push(Token::Operator(Operator::Not)),
							    "||" = > tokens.push(Token::Operator(Operator::Or)),
							    "&&" = > tokens.push(Token::Operator(Operator::And)),
							    "%" = > tokens.push(Token::Operator(Operator::Mod)),
							    "=" = > tokens.push(Token::Operator(Operator::Assign)),
							    "->" = > tokens.push(Token::FuncRet),
							    _ = > panic !("Weird operator"),
							} mode = Mode::Select;
						}
					}
					Mode::Stringy = > {
						if ch
							== '"' || ch == '\'' {
								tokens.push(Token::Stringy(buffer.clone()));
								state.advance();
								mode = Mode::Select;
							}
						else {
							buffer.push(ch);
							state.advance();
						}
					}
					Mode::Identifier = > {
						if is_op (ch)
							|| ch == ' ' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '"' || ch == '\'' || ch == ';' || ch == '.' ||
							    ch == ',' || ch == ':' || ch == '\n' || ch == '\r' {
								push_identifier(&mut tokens, &buffer);
								mode = Mode::Select;
							}
						else {
							buffer.push(ch);
							state.advance();
						}
					}
					Mode::Attribute = > {
						if ch
							== ']' {
								push_attribute(&mut tokens, &buffer);
								state.advance();
								mode = Mode::Select;
							}
						else {
							buffer.push(ch);
							state.advance();
						}
					}
				}
			}
		}
	}
}
