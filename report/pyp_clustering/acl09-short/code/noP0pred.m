function output = antoniakpred(input,b)

uniqin = unique(input);
prediction = zeros(max(input),1);

for i = 1:length(uniqin)
  prediction(uniqin(i)) = b*log((b+uniqin(i))/b);
end

output = prediction(input);

