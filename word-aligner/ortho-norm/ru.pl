#!/usr/bin/perl -w
use strict;
use utf8;
binmode(STDIN,":utf8");
binmode(STDOUT,":utf8");
while(<STDIN>) {
  $_ = uc $_;
  s/А/a/g;
  s/І/i/g;
  s/Б/b/g;
  s/В/v/g;
  s/Г/g/g;
  s/Д/d/g;
  s/Е/e/g;
  s/Ж/zh/g;
  s/З/z/g;
  s/И/i/g;
  s/Й/i/g;
  s/К/k/g;
  s/Л/l/g;
  s/М/m/g;
  s/Н/n/g;
  s/О/o/g;
  s/П/p/g;
  s/Р/r/g;
  s/С/s/g;
  s/Т/t/g;
  s/У/u/g;
  s/Ф/f/g;
  s/Х/kh/g;
  s/Ц/c/g;
  s/Ч/ch/g;
  s/Ш/sh/g;
  s/Щ/shch/g;
  s/Ъ//g;
  s/Ы//g;
  s/Ь//g;
  s/Э/e/g;
  s/Ю/yo/g;
  s/Я/ya/g;
  $_ = lc $_;
  print;
}

