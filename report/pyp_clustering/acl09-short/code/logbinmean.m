function [ logbinsvalid , meanval, seval ] = logbinmean( frequency, typecount, NBINS , MinCounts );

% calculate distribution of frequency
Maxfrequency = max( frequency );
meanK  = mean( frequency );
linbins = linspace( log10(1) , log10( Maxfrequency ) , NBINS );
stepb   = linbins( 2 ) - linbins( 1 );

logbins  = 10.^linbins;

% !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
LL       = length( linbins ) - 1;
for i=1:LL
  lowb  = linbins( i   );
  highb = linbins( i+1 );
  linbinsout( i ) = (highb + lowb) / 2;
  
  lowb  = logbins( i   );
  highb = logbins( i+1 );
  step  = highb - lowb;
  logbinsout( i ) = 10^linbinsout( i );
  
  indices = find( frequency >= lowb & frequency < highb);
  
  meanval(i) = mean(typecount(indices));
  rawcounts(i) = length(indices);
  seval(i) = std(typecount(indices))./sqrt(rawcounts(i));
  
end

valid = 1:LL;
valid( find( rawcounts <= MinCounts )) = [];

linbinsvalid   = linbinsout( valid );
logbinsvalid   = logbinsout( valid );

meanval     =  meanval( valid );
seval       =  seval( valid );
