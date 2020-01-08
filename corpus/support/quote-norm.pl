#!/usr/bin/perl -w
$|++;
use strict;
use utf8;
binmode(STDIN,"utf8");
binmode(STDOUT,"utf8");
while(<STDIN>) {
  chomp;
  $_ = " $_ ";

  # Delete control characters:
  s/[\x{00}-\x{1f}]//g; 

  # PTB --> normal
  s/-LRB-/(/g;
  s/-RRB-/)/g;
  s/-LSB-/[/g;
  s/-RSB-/]/g;
  s/-LCB-/{/g;
  s/-RCB-/}/g;
  s/ gon na / gonna /g;

  # Regularize named HTML/XML escapes:
  s/&\s*lt\s*;/</gi;    # HTML opening angle bracket
  s/&\s*gt\s*;/>/gi;    # HTML closing angle bracket
  s/&\s*squot\s*;/'/gi; # HTML single quote
  s/&\s*quot\s*;/"/gi;  # HTML double quote
  s/&\s*nbsp\s*;/ /gi;  # HTML non-breaking space
  s/&apos;/\'/g;        # HTML apostrophe
  s/&\s*amp\s*;/&/gi;   # HTML ampersand (last)

  # Regularize known HTML numeric codes:
  s/&\s*#\s*160\s*;/ /gi;           # no-break space
  s/&\s*#45\s*;\s*&\s*#45\s*;/--/g; # hyphen-minus hyphen-minus
  s/&\s*#45\s*;/--/g;               # hyphen-minus

  # Convert arbitrary hex or decimal HTML entities to actual characters:
  s/&\#x([0-9A-Fa-f]+);/pack("U", hex($1))/ge;
  s/&\#([0-9]+);/pack("U", $1)/ge;

  # Regularlize spaces:
  s/\x{ad}//g;        # soft hyphen
  s/\x{200C}//g;      # zero-width non-joiner
  s/\x{a0}/ /g;       # non-breaking space
  s/\x{2009}/ /g;     # thin space
  s/\x{2028}/ /g;     # "line separator"
  s/\x{2029}/ /g;     # "paragraph separator"
  s/\x{202a}/ /g;     # "left-to-right embedding"
  s/\x{202b}/ /g;     # "right-to-left embedding"
  s/\x{202c}/ /g;     # "pop directional formatting"
  s/\x{202d}/ /g;     # "left-to-right override"
  s/\x{202e}/ /g;     # "right-to-left override"
  s/\x{85}/ /g;       # "next line"
  s/\x{fffd}/ /g;     # "replacement character"
  s/\x{feff}/ /g;     # byte-order mark
  s/\x{fdd3}/ /g;     # "unicode non-character"

  # Convert other Windows 1252 characters to UTF-8 
  s/\x{80}/\x{20ac}/g;    # euro sign
  s/\x{95}/\x{2022}/g;    # bullet
  s/\x{99}/\x{2122}/g;    # trademark sign

  # Currency and measure conversions:
  s/ (\d\d): (\d\d)/ $1:$2/g;
  s/[\x{20a0}]\x{20ac}]/ EUR /g;
  s/[\x{00A3}]/ GBP /g;
  s/(\W)([A-Z]+\$?)(\d*\.\d+|\d+)/$1$2 $3/g;
  s/(\W)(euro?)(\d*\.\d+|\d+)/$1EUR $3/gi;

  # Ridiculous double conversions, UTF8 -> Windows 1252 -> UTF8:
  s/ï¿½c/--/g;                        # long dash
  s/\x{e2}\x{20ac}oe/\"/g;            # opening double quote
  s/\x{e2}\x{20ac}\x{9c}/\"/g;        # opening double quote
  s/\x{e2}\x{20ac}\x{9d}/\"/g;        # closing double quote
  s/\x{e2}\x{20ac}\x{2122}/\'/g;      # apostrophe
  s/\x{e2}\x{20ac}\x{201c}/ -- /g;    # en dash?
  s/\x{e2}\x{20ac}\x{201d}/ -- /g;    # em dash? 
  s/â(\x{80}\x{99}|\x{80}\x{98})/'/g; # single quote?
  s/â(\x{80}\x{9c}|\x{80}\x{9d})/"/g; # double quote?
  s/\x{c3}\x{9f}/\x{df}/g;            # esset
  s/\x{c3}\x{0178}/\x{df}/g;          # esset
  s/\x{c3}\x{a4}/\x{e4}/g;            # a umlaut
  s/\x{c3}\x{b6}/\x{f6}/g;            # o umlaut
  s/\x{c3}\x{bc}/\x{fc}/g;            # u umlaut
  s/\x{c3}\x{84}/\x{c4}/g;            # A umlaut: create no C4s after this
  s/\x{c3}\x{201e}/\x{c4}/g;          # A umlaut: create no C4s after this
  s/\x{c3}\x{96}/\x{d6}/g;            # O umlaut
  s/\x{c3}\x{2013}/\x{d6}/g;          # O umlaut
  s/\x{c3}\x{bc}/\x{dc}/g;            # U umlaut
  s/\x{80}/\x{20ac}/g;                # euro sign
  s/\x{95}/\x{2022}/g;                # bullet
  s/\x{99}/\x{2122}/g;                # trademark sign

  # Regularize quotes:
  s/ˇ/'/g;            # caron
  s/´/'/g;            # acute accent
  s/`/'/g;            # grave accent
  s/ˉ/'/g;            # modified letter macron
  s/ ,,/ "/g;         # ghetto low-99 quote
  s/``/"/g;           # latex-style left quote
  s/''/"/g;           # latex-style right quote
  s/\x{300c}/"/g;     # left corner bracket
  s/\x{300d}/"/g;     # right corner bracket
  s/\x{3003}/"/g;     # ditto mark
  s/\x{00a8}/"/g;     # diaeresis
  s/\x{92}/\'/g;      # curly apostrophe
  s/\x{2019}/\'/g;    # curly apostrophe
  s/\x{f03d}/\'/g;    # curly apostrophe
  s/\x{b4}/\'/g;      # curly apostrophe
  s/\x{2018}/\'/g;    # curly single open quote
  s/\x{201a}/\'/g;    # low-9 quote
  s/\x{93}/\"/g;      # curly left quote
  s/\x{201c}/\"/g;    # curly left quote
  s/\x{94}/\"/g;      # curly right quote
  s/\x{201d}/\"/g;    # curly right quote
  s/\x{2033}/\"/g;    # curly right quote
  s/\x{201e}/\"/g;    # low-99 quote
  s/\x{84}/\"/g;      # low-99 quote (bad enc)
  s/\x{201f}/\"/g;    # high-rev-99 quote
  s/\x{ab}/\"/g;      # opening guillemet
  s/\x{bb}/\"/g;      # closing guillemet
  s/\x{0301}/'/g;     # combining acute accent
  s/\x{203a}/\"/g;    # angle quotation mark
  s/\x{2039}/\"/g;    # angle quotation mark

  # Space inverted punctuation:
  s/¡/ ¡ /g;
  s/¿/ ¿ /g;

  # Russian abbreviations:
  s/ п. п. / п.п. /g;
  s/ ст. л. / ст.л. /g;
  s/ т. е. / т.е. /g;
  s/ т. к. / т.к. /g;
  s/ т. ч. / т.ч. /g;
  s/ т. д. / т.д. /g;
  s/ т. п. / т.п. /g;
  s/ и. о. / и.о. /g;
  s/ с. г. / с.г. /g;
  s/ г. р. / г.р. /g;
  s/ т. н. / т.н. /g;
  s/ т. ч. / т.ч. /g;
  s/ н. э. / н.э. /g;

  # Convert foreign numerals into Arabic numerals
  tr/०-९/0-9/; # devangari
  tr/౦-౯/0-9/; # telugu
  tr/೦-೯/0-9/; # kannada
  #tr/೦-௯/0-9/; # tamil
  tr/൦-൯/0-9/; # malayalam

  # Random punctuation:
  tr/！-～/!-~/;
  s/、/,/g;
  # s/。/./g;
  s/\x{85}/.../g;
  s/…/.../g;
  s/―/--/g;
  s/–/--/g;
  s/─/--/g;
  s/—/--/g;
  s/\x{97}/--/g;
  s/•/ * /g;
  s/\*/ * /g;
  s/،/,/g;
  s/؟/?/g;
  s/ـ/ /g;
  s/Ã ̄/i/g;
  s/â€™/'/g;
  s/â€"/"/g;
  s/؛/;/g;

  # Regularize ligatures:
  s/\x{9c}/oe/g;      # "oe" ligature 
  s/\x{0153}/oe/g;    # "oe" ligature 
  s/\x{8c}/Oe/g;      # "OE" ligature
  s/\x{0152}/Oe/g;    # "OE" ligature
  s/\x{fb00}/ff/g;    # "ff" ligature
  s/\x{fb01}/fi/g;    # "fi" ligature
  s/\x{fb02}/fl/g;    # "fl" ligature
  s/\x{fb03}/ffi/g;   # "ffi" ligature
  s/\x{fb04}/ffi/g;   # "ffl" ligature

  s/β/ß/g; # WMT 2010 error

  # Strip extra spaces: 
  s/\s+/ /g;
  s/^\s+//;
  s/\s+$//;

  print "$_\n";
}

