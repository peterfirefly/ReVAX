# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.
#
# ---
#
# Operand specification for asm/dis/sim -- see src/operands.pl
#
# print out with something like:
#   a2ps --prologue=bold -1 --landscape --lines-per-page=xx src/operands.spec

[asm.vax]
# Reg
# Idx
# Rn
# Rx
#

#                                          VAX
00xxxxxx=lit6                      | <lit6:lit6>

0100xxxx=Rx 0110xxxx=Rn		   | (_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 0111xxxx=Rn		   | -(_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 1000xxxx=Rn		   | (_<Rn:reg>_)+[_<Rx:reg>_]
0100xxxx=Rx 10011111	<addr:32>  | @#<addr:addr>[_<Rx:reg>_]
0100xxxx=Rx 1001xxxx=Rn		   | @(_<Rn:reg>_)+[_<Rx:reg>_]
0100xxxx=Rx 10101111	<disp:8>   | B^<disp:disp>(_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 1010xxxx=Rn	<disp:8>   | B^<disp:disp>(_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 10111111	<disp:8>   | @B^<disp:disp>(_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 1011xxxx=Rn	<disp:8>   | @B^<disp:disp>(_<Rn:reg>_)[_<Rx:reg>_]
0100xxxx=Rx 11001111	<disp:16>  | ...
0100xxxx=Rx 1100xxxx=Rn	<disp:16>  | ...
0100xxxx=Rx 11011111	<disp:16>  | ...
0100xxxx=Rx 1101xxxx=Rn	<disp:16>  | ...
0100xxxx=Rx 11101111    <disp:32>  | ...
0100xxxx=Rx 1110xxxx=Rn <disp:32>  | ...
0100xxxx=Rx 11111111    <disp:32>  | ...
0100xxxx=Rx 1111xxxx=Rn <disp:32>  | ...

0101xxxx=Rn 		           | <Rn:reg>
0110xxxx=Rn 		           | (_<Rn:reg>_)
0111xxxx=Rn		           | -(_<Rn:reg>_)
10001111    <imm:width>		   | <imm:imm>
1000xxxx=Rn			   | ...
10011111    <addr:32>		   | @#<addr:addr>
1001xxxx=Rn			   | ...
10101111    <disp:8>		   | ...
1010xxxx=Rn <disp:8>		   | ...
10111111    <disp:8>		   | ...
1011xxxx=Rn <disp:8>		   | ...
11001111    <disp:16>		   | ...
1100xxxx=Rn <disp:16>		   | ...
11011111    <disp:16>		   | ...
1101xxxx=Rn <disp:16>		   | ...
11101111    <disp:32>		   | ...
1110xxxx=Rn <disp:32>		   | ...
11111111    <disp:32>		   | ...
1111xxxx=Rn <disp:32>		   | ...


[asm.sane]
# Reg
# Idx
# Rn
# Rx
#

#                                       SANE
00xxxxxx=lit6                      |   <lit6:lit6>

0100xxxx=Rx 0110xxxx=Rn		   |   [_<Rn:reg>_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 0111xxxx=Rn		   |   [_--<Rn:reg>_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1000xxxx=Rn		   |   [_<Rn:reg>++_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 10011111	<addr:32>  |   [_<addr:addr>_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1001xxxx=Rn		   | [_[_<Rn:reg>++_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 10101111	<disp:8>   |   [_PC_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1010xxxx=Rn	<disp:8>   |   [_<Rn:reg>_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 10111111	<disp:8>   | [_[_PC_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1011xxxx=Rn	<disp:8>   | [_[_<Rn:reg>_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 11001111	<disp:16>  |   [_PC_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1100xxxx=Rn	<disp:16>  |   [_<Rn:reg>_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 11011111	<disp:16>  | [_[_PC_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1101xxxx=Rn	<disp:16>  | [_[_<Rn:reg>_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 11101111    <disp:32>  |   [_PC_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1110xxxx=Rn <disp:32>  |   [_<Rn:reg>_+_<disp:disp>___+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 11111111    <disp:32>  | [_[_PC_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]
0100xxxx=Rx 1111xxxx=Rn <disp:32>  | [_[_<Rn:reg>_+_<disp:disp>_]_+_<Rx:reg>_*_<width>_]

0101xxxx=Rn 		           | <Rn:reg>
0110xxxx=Rn 		           | [_<Rn:reg>_]
0111xxxx=Rn		           | [_--<Rn:reg>_]
10001111    <imm:width>		   | <imm:imm>
1000xxxx=Rn			   | [<Rn:reg>++]
10011111    <addr:32>		   | [_<addr:addr>_]
1001xxxx=Rn			   | [_[_<Rn:reg>++_]_]
10101111    <disp:8>		   |  [_PC_+_<disp:disp>_]
1010xxxx=Rn <disp:8>		   |  [_<Rn:reg>_+_<disp:disp>_]
10111111    <disp:8>		   | [_[_PC_+_<disp:disp>_]_]
1011xxxx=Rn <disp:8>		   | [_[_<Rn:reg>_+_<disp:disp>_]_]
11001111    <disp:16>		   |  [_PC_+_<disp:disp>_]
1100xxxx=Rn <disp:16>		   |  [_<Rn:reg>_+_<disp:disp>_]
11011111    <disp:16>		   | [_[_PC_+_<disp:disp>_]_]
1101xxxx=Rn <disp:16>		   | [_[_<Rn:reg>_+_<disp:disp>_]_]
11101111    <disp:32>		   |  [_PC_+_<disp:disp>_]
1110xxxx=Rn <disp:32>		   |  [_<Rn:reg>_+_<disp:disp>_]
11111111    <disp:32>		   | [_[_PC_+_<disp:disp>_]_]
1111xxxx=Rn <disp:32>		   | [_[_<Rn:reg>_+_<disp:disp>_]_]


[dis.vax]
# @Reg
# @Idx
# @Rn
# @Rx
#
# expand_lit6 expands the lit6 field to imm[] according to width/ifp

#                                          VAX
00xxxxxx=lit6/expand_lit6             | <imm:imm>

0100xxxx=Rx 0110xxxx=Rn		      | (<Rn:reg>)[<Rx:reg>]
0100xxxx=Rx 0111xxxx=Rn		      | -(<Rn:reg>)[<Rx:reg>]
0100xxxx=Rx 1000xxxx=Rn		      | (<Rn:reg>)+[<Rx:reg>]
0100xxxx=Rx 10011111    <addr:32>     | @#<addr:addr>[<Rx:reg>]
0100xxxx=Rx 1001xxxx=Rn		      | @(<Rn:reg>)+[<Rx:reg>]
0100xxxx=Rx 10101111    <disp:8>      | ...
0100xxxx=Rx 1010xxxx=Rn <disp:8>      | ...
0100xxxx=Rx 10111111	<disp:8>      | ...
0100xxxx=Rx 1011xxxx=Rn	<disp:8>      | ...
0100xxxx=Rx 11001111    <disp:16>     | ...
0100xxxx=Rx 1100xxxx=Rn <disp:16>     | ...
0100xxxx=Rx 11011111    <disp:16>     | ...
0100xxxx=Rx 1101xxxx=Rn <disp:16>     | ...
0100xxxx=Rx 11101111	<disp:32>     | ...
0100xxxx=Rx 1110xxxx=Rn	<disp:32>     | ...
0100xxxx=Rx 11111111	<disp:32>     | ...
0100xxxx=Rx 1111xxxx=Rn	<disp:32>     | ...

0101xxxx=Rn 		         | <Rn:reg>
0110xxxx=Rn 		         | (<Rn:reg>)
0111xxxx=Rn 		         | (-<Rn:reg>)
10001111    <imm:width>		 | <imm:imm>
1000xxxx=Rn			 | (<Rn:reg>+)
10011111    <addr:32>		 | @#<addr:label>
1001xxxx=Rn			 | @(<Rn:reg>)
10101111    <disp:8>		 | ...
1010xxxx=Rn <disp:8>		 | B^<disp:disp>(<Rn:reg>)
10111111    <disp:8>		 | ...
1011xxxx=Rn <disp:8>		 | @B^<disp:disp>(<Rn:reg>)
11001111    <disp:16>		 | ...
1100xxxx=Rn <disp:16>		 | W^<disp:disp>(<Rn:reg>)
11011111    <disp:16>		 | ...
1101xxxx=Rn <disp:16>		 | @W^<disp:disp>(<Rn:reg>)
11101111    <disp:32>		 | (PC+<disp:disp>)
1110xxxx=Rn <disp:32>		 | L^<disp:disp>(<Rn:reg>)
11111111    <disp:32>		 | ...
1111xxxx=Rn <disp:32>		 | @L^<disp:disp>(<Rn:reg>)


[dis.sane]
# @Reg
# @Idx
# @Rn
# @Rx
#
# expand_lit6 expands the lit6 field to imm[] according to width/ifp

#                                          SANE
00xxxxxx=lit6/expand_lit6             | <imm:imm>

0100xxxx=Rx 0110xxxx=Rn		      | [<Rn:reg>+<Rx:reg>*<width>]
0100xxxx=Rx 0111xxxx=Rn		      | [--<Rn:reg>+<Rx:reg>*<width>]
0100xxxx=Rx 1000xxxx=Rn		      | [<Rn:reg>++ + <Rx:reg>*<width>]
0100xxxx=Rx 10011111    <addr:32>     | [<addr:addr>+<Rx:reg>*<width>]
0100xxxx=Rx 1001xxxx=Rn		      | [[<Rn:reg>++]+<Rx:reg>*<width>]
0100xxxx=Rx 10101111    <disp:8>      |  [PC+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 1010xxxx=Rn <disp:8>      |  [<Rn:reg>+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 10111111	<disp:8>      | [[PC+<disp:disp>]+<Rx:reg>*<width>]
0100xxxx=Rx 1011xxxx=Rn	<disp:8>      | [[<Rn:reg>+<disp:disp>]+<Rx:reg>*<width>]
0100xxxx=Rx 11001111    <disp:16>     |  [PC+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 1100xxxx=Rn <disp:16>     |  [<Rn:reg>+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 11011111    <disp:16>     | [[PC+<disp:disp>]+<Rx:reg>*<width>]
0100xxxx=Rx 1101xxxx=Rn <disp:16>     | [[<Rn:reg>+<disp:disp>]+<Rx:reg>*<width>]
0100xxxx=Rx 11101111	<disp:32>     |  [PC+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 1110xxxx=Rn	<disp:32>     |  [<Rn:reg>+<disp:disp>+<Rx:reg>*<width>]
0100xxxx=Rx 11111111	<disp:32>     | [[PC+<disp:disp>]+<Rx:reg>*<width>]
0100xxxx=Rx 1111xxxx=Rn	<disp:32>     | [[<Rn:reg>+<disp:disp>]+<Rx:reg>*<width>]

0101xxxx=Rn 		         | <Rn:reg>
0110xxxx=Rn 		         | [<Rn:reg>]
0111xxxx=Rn 		         | [--<Rn:reg>]
10001111    <imm:width>		 | <imm:imm>
1000xxxx=Rn			 | [<Rn:reg>++]
10011111    <addr:32>		 | [<addr:label>]
1001xxxx=Rn			 | [[<Rn:reg>++]]
10101111    <disp:8>		 |  [PC+<disp:disp>]
1010xxxx=Rn <disp:8>		 |  [<Rn:reg>+<disp:disp>]
10111111    <disp:8>		 | [[PC+<disp:disp>]]
1011xxxx=Rn <disp:8>		 | [[<Rn:reg>+<disp:disp>]]
11001111    <disp:16>		 |  [PC+<disp:disp>]
1100xxxx=Rn <disp:16>		 |  [<Rn:reg>+<disp:disp>]
11011111    <disp:16>		 | [[PC+<disp:disp>]]
1101xxxx=Rn <disp:16>		 | [[<Rn:reg>+<disp:disp>]]
11101111    <disp:32>		 |  [PC+<disp:disp>]
1110xxxx=Rn <disp:32>		 |  [<Rn:reg>+<disp:disp>]
11111111    <disp:32>		 | [[PC+<disp:disp>]]
1111xxxx=Rn <disp:32>		 | [[<Rn:reg>+<disp:disp>]]


[sim]
# @Reg checks that Rn + (width+3)/4 - 1 < PC
# @Idx checks that Rn != Rx, Rx != PC, Rn != PC
# @Rn checks that Rn != PC
# @Rx checks that Rx != PC
#
# expand_lit6 expands the imm6 field to imm[] according to width/ifp

00xxxxxx=lit6/expand_lit6		 | I

0100xxxx=Rx 0110xxxx=Rn		         | M: [Rn    + index(Rx)]
0100xxxx=Rx 0111xxxx=Rn		         | M: [--Rn  + index(Rx)]
0100xxxx=Rx 1000xxxx=Rn		         | M: [Rn++  + index(Rx)]
0100xxxx=Rx 10011111  	<addr:32>        | M: [addr  + index(Rx)]
0100xxxx=Rx 1001xxxx=Rn		         | M:[[Rn++] + index(Rx)]
0100xxxx=Rx 10101111	<disp:8>         | M:[ PC+disp  + index(Rx)]
0100xxxx=Rx 1010xxxx=Rn	<disp:8>         | M:[ Rn+disp  + index(Rx)]
0100xxxx=Rx 10111111	<disp:8>         | M:[[PC+disp] + index(Rx)]
0100xxxx=Rx 1011xxxx=Rn	<disp:8>         | M:[[Rn+disp] + index(Rx)]
0100xxxx=Rx 11001111	<disp:16>        | M:[ PC+disp  + index(Rx)]
0100xxxx=Rx 1100xxxx=Rn	<disp:16>        | M:[ Rn+disp  + index(Rx)]
0100xxxx=Rx 11011111	<disp:16>        | M:[[PC+disp] + index(Rx)]
0100xxxx=Rx 1101xxxx=Rn	<disp:16>        | M:[[Rn+disp] + index(Rx)]
0100xxxx=Rx 11101111	<disp:32>        | M:[ PC+disp  + index(Rx)]
0100xxxx=Rx 1110xxxx=Rn	<disp:32>        | M:[ Rn+disp  + index(Rx)]
0100xxxx=Rx 11111111	<disp:32>        | M:[[PC+disp] + index(Rx)]
0100xxxx=Rx 1111xxxx=Rn	<disp:32>        | M:[[Rn+disp] + index(Rx)]

0101xxxx=Rn			         | R
0110xxxx=Rn 			         | M:[Rn]
0111xxxx=Rn 			         | M:[--Rn]
10001111    <imm:width>		         | I
1000xxxx=Rn			         | M:[Rn++]
10011111    <addr:32>		 	 | M:[addr]
1001xxxx=Rn			         | M:[[Rn++]]
10101111    <disp:8>		         | M:[PC+disp]
1010xxxx=Rn <disp:8>		         | M:[Rn+disp]
10111111    <disp:8>		         | M:[[PC+disp]]
1011xxxx=Rn <disp:8>		         | M:[[Rn+disp]]
11001111    <disp:16>		         | M:[PC+disp]
1100xxxx=Rn <disp:16>		         | M:[Rn+disp]
11011111    <disp:16>		         | M:[[PC+disp]]
1101xxxx=Rn <disp:16>		         | M:[[Rn+disp]]
11101111    <disp:32>		         | M:[PC+disp]
1110xxxx=Rn <disp:32>		         | M:[Rn+disp]
11111111    <disp:32>		         | M:[[PC+disp]]
1111xxxx=Rn <disp:32>		         | M:[[Rn+disp]]



[val]
# lists patterns that are (conditionally) invalid
#
# @reg checks that Rn + (width+3)/4 - 1 < PC
# @idx checks that Rn != Rx
#
# Why these restrictions?  The addressing modes has to make sense and they have
# to be implementable on very simple sequentially decoding machines with no
# effective pipelining and on machines with sophisticated machines with
# pipelining.
#
# Therefore, PC modifying addressing modes are only allowed in a select few
# places + indexed addressing modes that use inc/dec must use different Rx and
# Rn registers.
#
# 5F in "rb/rw/rl" could make sense but it would be a special case for a pipe-
# lined machine.
# 6F would also be a special case.
#
# 7F makes no sense.


# 4F        never allowed

# 4_ 00..3F never allowed
# 4_ 4_     never allowed
# 4_ 5_     never allowed

# 4_ 6F     never allowed
# 4_ 7F     never allowed
# 4_ 8F     never allowed

# 4_ 7_     only allowed with Rx != Rn
# 4_ 8_     only allowed with Rx != Rn
# 4_ 9_     only allowed with Rx != Rn

# 5F        never allowed, sometimes 5C/5D/5E not allowed either (depends on width)
# 6F        never allowed
# 7F        never allowed

01001111

0100____    00______
0100____    0100____
0100____    0101____

0100____    01101111
0100____    01111111
0100____    10001111

0100xxxx=Rx 0111xxxx=Rn			@idx
0100xxxx=Rx 1000xxxx=Rn			@idx
0100xxxx=Rx 1001xxxx=Rn			@idx

0101xxxx=Rn				@reg
01101111
01111111

