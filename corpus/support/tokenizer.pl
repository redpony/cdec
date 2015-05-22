#!/usr/bin/env perl
$|++;

my $script_dir;
BEGIN {$^W = 1; use Cwd qw/ abs_path /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, $script_dir; }  

use strict;
use utf8;

binmode STDIN, ":utf8";
binmode STDOUT, ":utf8";
binmode STDERR, ":utf8";

my $debug = 0;


############ options: 
### for all options:
### 0 means no split on that symbol
### 1 means split on that symbol in all cases.
### 2 means do not split in condition 1.
### n means do not split in any of the conditions in the set {1, 2, ..., n-1}.


### prefix
## for "#": #90
my $Split_On_SharpSign = 2; # 2: do not split on Num, e.g., "#90"


############## "infix"
my $Split_On_Tilde = 2;  # 2: do not split on Num, e.g., "12~13".

my $Split_On_Circ = 2;   # 2: do not split on Num, e.g, "2^3"

## for "&"
my $Split_On_AndSign = 2;  # 2: do not split on short Name, e.g., "AT&T".

## for hyphen: 1990-1992
my $Split_On_Dash = 2;  ## 2: do not split on number, e.g., "22-23".
my $Split_On_Underscore = 0;  ## 0: do not split by underline

## for ":": 5:4
my $Split_On_Semicolon = 2; ## 2: don't split for num, e.g., "5:4"

###########  suffix
## for percent sign: 5%
my $Split_On_PercentSign = 1;      ## 2: don't split num, e.g., 5%

############# others
## for slash: 1/4
my $Split_On_Slash = 2;  ## 2: don't split on number, e.g., 1/4.
my $Split_On_BackSlash = 0;  ## 0: do not split on "\", e.g., \t

### for "$": US$120
my $Split_On_DollarSign = 2; ### 2: US$120 => "US$ 120"
                             ### 1: US$120 => "US $ 120"
## for 's etc.
my $Split_NAposT = 1;  ## n't   
my $Split_AposS = 1;   ## 's
my $Split_AposM = 1;   ## 'm
my $Split_AposRE = 1;  ## 're
my $Split_AposVE = 1;  ## 've
my $Split_AposLL = 1;  ## 'll
my $Split_AposD  = 1;  ## 'd


### some patterns
my $common_right_punc = '\x{0964}|\.|\,|\;|\!|:|\?|\"|\)|\]|\}|\>|\-';

#### step 1: read files

my $workdir = $script_dir;
my $dict_file = "$workdir/token_list";
my $word_patt_file = "$workdir/token_patterns";

open(my $dict_fp, "$dict_file") or die;
binmode($dict_fp, ":utf8");

# read in the list of words that should not be segmented, 
##  e.g.,"I.B.M.", co-operation.
my %dict_hash = ();
my $dict_entry = 0;
while(<$dict_fp>){
    chomp;
    next if /^\s*$/;
    s/^\s+//;
    s/\s+$//;
    tr/A-Z/a-z/;
    $dict_hash{$_} = 1;
    $dict_entry ++;
}

open(my $patt_fp, "$word_patt_file") or die;
binmode($patt_fp, ":utf8");
my @word_patts = ();
my $word_patt_num = 0;
while(<$patt_fp>){
    chomp;
    next if /^\s*$/;
    s/^\s+//;
    s/\s+$//;
    s/^\/(.+)\/$/$1/;   # remove / / around the pattern
    push(@word_patts, $_);
    $word_patt_num ++;
}


###### step 2: process the input file
my $orig_token_total = 0;
my $deep_proc_token_total = 0;
my $new_token_total = 0;

while(<STDIN>){
    chomp();
    s/\x{0970}/./g;  # dev abbreviation character
    if(/^(\[b\s+|\]b|\]f|\[f\s+)/ || (/^\[[bf]$/) || (/^\s*$/) || /^<DOC/ || /^<\/DOC/) {
	## markup
	print STDOUT "$_\n";
	next;
    }

    my $orig_num = 0;
    my $deep_proc_num = 0;
    s/(\x{0964}+)/ $1/g;  # Devangari end of sentence
    my $new_line = proc_line($_, \$orig_num, \$deep_proc_num);

    $orig_token_total += $orig_num;
    $deep_proc_token_total += $deep_proc_num;

    $new_line =~ s/\s+$//;
    $new_line =~ s/^\s+//;
    my @parts = split(/\s+/, $new_line);
    $new_token_total += scalar @parts;

    $new_line =~ s/\s+/ /g;
# fix sgm-markup tokenization
    $new_line =~ s/\s*<\s+seg\s+id\s+=\s+(\d+)\s+>/<seg id=$1>/;
    $new_line =~ s/\s*<\s+(p|hl)\s+>/<$1>/;
    $new_line =~ s/\s*<\s+\/\s+(p|hl|DOC)\s+>/<\/$1>/;
    $new_line =~ s/<\s+\/\s+seg\s+>/<\/seg>/;
    if ($new_line =~ /^\s*<\s+DOC\s+/) {
	$new_line =~ s/\s+//g;
	$new_line =~ s/DOC/DOC /;
	$new_line =~ s/sys/ sys/;
    }
    if ($new_line =~ /^\s*<\s+(refset|srcset)\s+/) {
	$new_line =~ s/\s+//g;
	$new_line =~ s/(set|src|tgt|trg)/ $1/g;
    }

    chomp $new_line;
    print STDOUT "$new_line\n";
}

########################################################################
   
### tokenize a line. 
sub proc_line {
    my @params = @_;
    my $param_num = scalar @params;

    if(($param_num < 1) || ($param_num > 3)){
	die "wrong number of params for proc_line: $param_num\n";
    }

    my $orig_line = $params[0];

    $orig_line =~ s/^\s+//;
    $orig_line =~ s/\s+$//;

    my @parts = split(/\s+/, $orig_line);

    if($param_num >= 2){
	my $orig_num_ptr = $params[1];
	$$orig_num_ptr = scalar @parts;
    }

    my $new_line = "";

    my $deep_proc_token = 0;
    foreach my $part (@parts){
	my $flag = -1;
	$new_line .= proc_token($part, \$flag) . " ";
	$deep_proc_token += $flag;
    }

    if($param_num == 3){
	my $deep_num_ptr = $params[2];
	$$deep_num_ptr = $deep_proc_token;
    }

    return $new_line;
}



## Tokenize a str that does not contain " ", return the new string
## The function handles the cases that the token needs not be segmented.
## for other cases, it calls deep_proc_token()
sub proc_token {
    my @params = @_;
    my $param_num = scalar @params;
    if($param_num > 2){
	die "proc_token: wrong number of params: $param_num\n";
    }

    my $token = $params[0];

    if(!defined($token)){
	return "";
    }

    my $deep_proc_flag; 

    if($param_num == 2){
	$deep_proc_flag = $params[1];
        $$deep_proc_flag = 0;
    }

    if($debug){
	print STDERR "pro_token:+$token+\n";
    }

    ### step 0: it has only one char
    if(($token eq "") || ($token=~ /^.$/)){
	## print STDERR "see +$token+\n";
	return $token;
    }

    ## step 1: check the most common case
    if($token =~ /^[a-z0-9\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}\p{Devanagari}]+$/i){
    #if($token =~ /^[a-z0-9\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}]+$/i){
	### most common cases
	return $token;
    }

    ## step 2: check whether it is some NE entity
    ### 1.2.4.6
    if($token =~ /^\d+(.\d+)+$/){
	return $token;
    }

    if($token =~ /^\d+(.\d+)+(亿|百万|万|千)?$/){
	return $token;
    }

    ## 1,234,345.34
    if($token =~ /^\d+(\.\d{3})*,\d+$/){
	## number
	return $token;
    }
    if($token =~ /^\d+(,\d{3})*\.\d+$/){
	## number
	return $token;
    }
    if($token =~ /^(@|#)[A-Za-z0-9_\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}\p{Devanagari}]+.*$/){
        ## twitter hashtag or address
        return proc_rightpunc($token);
    }

    if($token =~ /^[a-z0-9\_\-]+\@[a-z\d\_\-]+(\.[a-z\d\_\-]+)*(.*)$/i){
	### email address: xxx@yy.zz
	return proc_rightpunc($token);
    }

    if($token =~ /^(mailto|http|https|ftp|gopher|telnet|file)\:\/{0,2}([^\.]+)(\.(.+))*$/i){
	### URL: http://xx.yy.zz
	return proc_rightpunc($token);
    }

    if($token =~ /^(www)(\.(.+))+$/i){
	###  www.yy.dd/land/
	return proc_rightpunc($token);
    }

    if($token =~ /^(\w+\.)+(com|co|edu|org|gov|ly|cz|ru|eu)(\.[a-z]{2,3})?\:{0,2}(\/\S*)?$/i){
	### URL: upenn.edu/~xx
	return proc_rightpunc($token);
    }

    if($token =~ /^\(\d{3}\)\d{3}(\-\d{4})($common_right_punc)*$/){
	## only handle American phone numbers: e.g., (914)244-4567
	return proc_rightpunc($token);
    }

    #my $t1 = '[\x{0600}-\x{06ff}a-z\d\_\.\-]';
    my $t1 = '[a-z\d\_\-\.\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}\p{Devanagari}]';
    if($token =~ /^\/(($t1)+\/)+($t1)+\/?$/i){
	### /nls/p/....
	return $token;
    }

    if($token =~ /^\\(($t1)+\\)+($t1)+\\?$/i){
	### \nls\p\....
	return $token;
    }
	
    ## step 3: check the dictionary
    my $token_lc = $token;
    $token_lc =~ tr/A-Z/a-z/;

    if(defined($dict_hash{$token_lc})){
	return $token;
    }

    ## step 4: check word_patterns
    my $i=1;
    foreach my $patt (@word_patts){
	if($token_lc =~ /$patt/){
	    if($debug){
		print STDERR "+$token+ match pattern $i: +$patt+\n";
	    }
	    return $token;
	}else{
	    $i++;
	}
    }

    ## step 5: call deep tokenization
    if($param_num == 2){
	$$deep_proc_flag = 1;
    }
    return deep_proc_token($token);
}


### remove punct on the right side
### e.g., xxx@yy.zz, => xxx@yy.zz ,
sub proc_rightpunc {
    my ($token) = @_;

    $token =~ s/(($common_right_punc)+)$/ $1 /;
    if($token =~ /\s/){
	return proc_line($token);
    }else{
	return $token;
    }
}
    


#######################################
### return the new token: 
###   types of punct:
##      T1 (2):   the punct is always a token by itself no matter where it
###           appears:   " ;  
##      T2 (15):  the punct that can be a part of words made of puncts only.
##               ` ! @ + = [ ] ( ) { } | < > ? 
##      T3 (15):  the punct can be part of a word that contains [a-z\d]
##        T3: ~ ^ & : , # * % - _ \ / . $ '
##             infix: ~ (12~13), ^ (2^3), & (AT&T),  : ,  
##             prefix: # (#9),  * (*3), 
##             suffix: % (10%), 
##             infix+prefix: - (-5), _ (_foo), 
##             more than one position: \ /  . $
##             Appos: 'm n't ...

##   1. separate by puncts in T1
##   2. separate by puncts in T2 
##   3. deal with punct T3 one by one according to options 
##   4. if the token remains unchanged after step 1-3, return the token

## $line contains at least 2 chars, and no space.
sub deep_proc_token {
    my ($line) = @_;
    if($debug){
	print STDERR "deep_proc_token: +$line+\n";
    }

    ##### step 0: if it mades up of all puncts, remove one punct at a time.
    if($line !~ /[\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}\p{Devanagari}a-zA-Z\d]/){
	if($line =~ /^(\!+|\@+|\++|\=+|\*+|\<+|\>+|\|+|\?+|\x{0964}+|\.+|\-+|\_+|\&+)$/){
	    ## ++ @@@@ !!! ....
	    return $line;
	}

	if($line =~ /^(.)(.+)$/){
	    my $t1 = $1;
	    my $t2 = $2;
	    return $t1 . " " . proc_token($t2);
	}else{
	    ### one char only
	    print STDERR "deep_proc_token: this should not happen: +$line+\n";
	    return $line;
	}
    }

    ##### step 1: separate by punct T2 on the boundary
    my $t2 = '\`|\!|\@|\+|\=|\[|\]|\<|\>|\||\(|\)|\{|\}|\?|\"|;|●|○';
    if($line =~ s/^(($t2)+)/$1 /){
        $line =~ s/"/“/;
	return proc_line($line);
    }
	
    if($line =~ s/(($t2)+)$/ $1/){
        $line =~ s/"/”/;
	return proc_line($line);
    }	

    ## step 2: separate by punct T2 in any position
    if($line =~ s/(($t2)+)/ $1 /g){
        $line =~ s/"/”/g;  # probably before punctuation char
	return proc_line($line);
    }

    ##### step 3: deal with special puncts in T3.
    if($line =~ /^(\,+)(.+)$/){
	my $t1 = $1;
	my $t2 = $2;
	return proc_token($t1) . " " . proc_token($t2);
    }

    if($line =~ /^(.*[^\,]+)(\,+)$/){
	## 19.3,,, => 19.3 ,,,
	my $t1 = $1;
	my $t2 = $2;
	return proc_token($t1) . " " . proc_token($t2);
    }

    ## remove the ending periods that follow number etc.
    if($line =~ /^(.*(\d|\~|\^|\&|\:|\,|\#|\*|\%|€|\-|\_|\/|\\|\$|\'))(\.+)$/){
	##    12~13. => 12~13 .
	my $t1 = $1;
	my $t3 = $3;
	return proc_token($t1) . " " . proc_token($t3);
    }

    ###  deal with "$"
    if(($line =~ /\$/) && ($Split_On_DollarSign > 0)){
	my $suc = 0;
	if($Split_On_DollarSign == 1){
	    ## split on all occasation
	    $suc = ($line =~ s/(\$+)/ $1 /g);
	}else{
	    ## split only between $ and number
	    $suc = ($line =~ s/(\$+)(\d)/$1 $2/g);
	}

	if($suc){
	    return proc_line($line);
	}
    }

    ## deal with "#"
    if(($line =~ /\#/) && ($Split_On_SharpSign > 0)){
	my $suc = 0;
	if($Split_On_SharpSign >= 2){
	    ### keep #50 as a token
	    $suc = ($line =~ s/(\#+)(\D)/ $1 $2/gi);
	}else{
	    $suc = ($line =~ s/(\#+)/ $1 /gi);
	}

	if($suc){
	    return proc_line($line);
	}
    }

    ## deal with '
    if($line =~ /\'/){
	my $suc = ($line =~ s/([^\'])([\']+)$/$1 $2/g);  ## xxx'' => xxx '' 
	
	### deal with ': e.g., 's, 't, 'm, 'll, 're, 've, n't

	##  'there => ' there   '98 => the same
	$suc += ($line =~ s/^(\'+)([a-z\p{Cyrillic}\p{Greek}\p{Hebrew}\p{Han}\p{Arabic}\p{Devanagari}]+)/ $1 $2/gi);
	
	##  note that \' and \. could interact: e.g.,  U.S.'s;   're.
	if($Split_NAposT && ($line =~ /^(.*[a-z]+)(n\'t)([\.]*)$/i)){
	    ## doesn't => does n't
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}

	## 's, 't, 'm,  'll, 're, 've: they've => they 've 
        ## 1950's => 1950 's     Co.'s => Co. 's
	if($Split_AposS && ($line =~ /^(.+)(\'s)(\W*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}

	if($Split_AposM && ($line =~ /^(.*[a-z]+)(\'m)(\.*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}


	if($Split_AposRE && ($line =~ /^(.*[a-z]+)(\'re)(\.*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}

	if($Split_AposVE && ($line =~ /^(.*[a-z]+)(\'ve)(\.*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}

	if($Split_AposLL && ($line =~ /^(.*[a-z]+)(\'ll)(\.*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);
	}

	if($Split_AposD && ($line =~ /^(.*[a-z]+)(\'d)(\.*)$/i)){
	    my $t1 = $1;
	    my $t2 = $2;
	    my $t3 = $3;
	    return proc_token($t1) . " " . $t2 . " " . proc_token($t3);	    
	}
	
	if($suc){
	    return proc_line($line);
	}
    }


    ## deal with "~"
    if(($line =~ /\~/) && ($Split_On_Tilde > 0)){
	my $suc = 0;
	if($Split_On_Tilde >= 2){
	    ## keep 12~13 as one token
	    $suc += ($line =~ s/(\D)(\~+)/$1 $2 /g);
	    $suc += ($line =~ s/(\~+)(\D)/ $1 $2/g);
	    $suc += ($line =~ s/^(\~+)(\d)/$1 $2/g);
	    $suc += ($line =~ s/(\d)(\~+)$/$1 $2/g);
	}else{
	    $suc += ($line =~ s/(\~+)/ $1 /g);	
	}
	if($suc){
	    return proc_line($line);
	}
    }

    ## deal with "^"
    if(($line =~ /\^/) && ($Split_On_Circ > 0)){
	my $suc = 0;
	if($Split_On_Circ >= 2){
	    ## keep 12~13 as one token
	    $suc += ($line =~ s/(\D)(\^+)/$1 $2 /g);
	    $suc += ($line =~ s/(\^+)(\D)/ $1 $2/g);
	}else{
	    $suc = ($line =~ s/(\^+)/ $1 /g);	
	}
	if($suc){
	    return proc_line($line);
	}
    }

    ## deal with ":"
    if(($line =~ /\:/) && ($Split_On_Semicolon > 0)){
	## 2: => 2 :
	my $suc = ($line =~ s/^(\:+)/$1 /);
	$suc += ($line =~ s/(\:+)$/ $1/);
	if($Split_On_Semicolon >= 2){
	    ## keep 5:4 as one token
	    $suc += ($line =~ s/(\D)(\:+)/$1 $2 /g);
	    $suc += ($line =~ s/(\:+)(\D)/ $1 $2/g);
	}else{
	    $suc += ($line =~ s/(\:+)/ $1 /g);	
	}

	if($suc){
	    return proc_line($line);
	}
    }

    ###  deal with hyphen: 1992-1993. 21st-24th
    if(($line =~ /\-/) && ($Split_On_Dash > 0)){
	my $suc = ($line =~ s/(\-{2,})/ $1 /g);
	if($Split_On_Dash >= 2){
	    ## keep 1992-1993 as one token
	    $suc += ($line =~ s/(\D)(\-+)/$1 $2 /g);
	    $suc += ($line =~ s/(\-+)(\D)/ $1 $2/g);
	}else{
	    ### always split on "-"
	    $suc += ($line =~ s/([\-]+)/ $1 /g);
	}

	if($suc){
	    return proc_line($line);
	}
    }

    ## deal with "_"
    if(($line =~ /\_/) && ($Split_On_Underscore > 0)){
	### always split on "-"
	if($line =~ s/([\_]+)/ $1 /g){
	    return proc_line($line);
	}
    }



    ## deal with "%"
    if(($line =~ /\%|€/) && ($Split_On_PercentSign > 0)){
	my $suc = 0;
	if($Split_On_PercentSign >= 2){
	    $suc += ($line =~ s/(\D)(\%+|€+)/$1 $2/g);
	}else{
	    $suc += ($line =~ s/(\%+|€+)/ $1 /g);
	}

	if($suc){
	    return proc_line($line);
	}
    }
	

    ###  deal with "/": 4/5
    if(($line =~ /\//) && ($Split_On_Slash > 0)){
	my $suc = 0;
	if($Split_On_Slash >= 2){
	    $suc += ($line =~ s/(\D)(\/+)/$1 $2 /g);
	    $suc += ($line =~ s/(\/+)(\D)/ $1 $2/g);
	}else{
	    $suc += ($line =~ s/(\/+)/ $1 /g);
	}

	if($suc){
	    return proc_line($line);
	}
    }


    ### deal with comma: 123,456
    if($line =~ /\,/){
	my $suc = 0;
	$suc += ($line =~ s/([^\d]),/$1 , /g);     ## xxx, 1923 => xxx , 1923
	$suc += ($line =~ s/\,\s*([^\d])/ , $1/g); ## 1923, xxx => 1923 , xxx

	$suc += ($line =~ s/,([\d]{1,2}[^\d])/ , $1/g);  ## 1,23 => 1 , 23
	$suc += ($line =~ s/,([\d]{4,}[^\d])/ , $1/g);  ## 1,2345 => 1 , 2345

	$suc += ($line =~ s/,([\d]{1,2})$/ , $1/g);  ## 1,23 => 1 , 23
	$suc += ($line =~ s/,([\d]{4,})$/ , $1/g);  ## 1,2345 => 1 , 2345

	if($suc){
	    return proc_line($line);
	}
    }
    

    ##  deal with "&"
    if(($line =~ /\&/) && ($Split_On_AndSign > 0)){
	my $suc = 0;
	if($Split_On_AndSign >= 2){
	    $suc += ($line =~ s/([a-z]{3,})(\&+)/$1 $2 /gi);
	    $suc += ($line =~ s/(\&+)([a-z]{3,})/ $1 $2/gi);
	}else{
	    $suc += ($line =~ s/(\&+)/ $1 /g);
	}

	if($suc){
	    return proc_line($line);
	}
    }
	
    ## deal with period
    if($line =~ /\./){
	if($line =~ /^(([\+|\-])*(\d+\,)*\d*\.\d+\%*)$/){
	    ### numbers: 3.5
	    return $line;
	}

        if ($line =~ /^(([a-z]|ए|बी|सी|डी|ई|एफ|जी|एच|आई|जे|के|एल|एम|एन|ओ|पी|क़यू|आर|एस|टी|यू|वी|डबल्यू|एक्स|वाई|ज़ेड|ज़ी)(\.([a-z]|ए|बी|सी|डी|ई|एफ|जी|एच|आई|जे|के|एल|एम|एन|ओ|पी|क़यू|आर|एस|टी|यू|वी|डबल्यू|एक्स|वाई|ज़ेड|ज़ी))+)(\.?)(\.*)$/i){
	    ## I.B.M.
	    my $t1 = $1 . $5;
	    my $t3 = $6;
	    return $t1 . " ". proc_token($t3);
	}

	## Feb.. => Feb. .
	if($line =~ /^(.*[^\.])(\.)(\.*)$/){
	    my $p1 = $1;
	    my $p2 = $2;
	    my $p3 = $3;
	    
	    my $p1_lc = $p1;
	    $p1_lc =~ tr/A-Z/a-z/;

	    if(defined($dict_hash{$p1_lc . $p2})){
		## Dec.. => Dec. .
		return $p1 . $p2 . " " . proc_token($p3);
	    }elsif(defined($dict_hash{$p1_lc})){
		return $p1 . " " . proc_token($p2 . $p3);
	    }else{
		## this. => this .
		return proc_token($p1) . " " . proc_token($p2 . $p3);
	    }
	}

	if($line =~ s/(\.+)(.+)/$1 $2/g){
	    return proc_line($line);
	}
    }


    ## no pattern applies
    return $line;
}

