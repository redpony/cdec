%wsj_lengths = load([ 'wsj_lengths.dat']);
%save([ 'wsj_lengths.mat'],'wsj_lengths');
load wsj
load wsj_lengths

figure(1)
clf 

hold on

%colors = [0 0 0; 0 0 1; 1 0 0; 0 1 0]; %pure black, red, blue, green
colors = [0 0 0; 1 .4 .2; .4 .4 1; 0 .7 .5]; %similar but less garish
%colors = [0 0 0; .6 .4 .4; .9 .6 .6; 1 .8 .8]; %shades of pink
%colors = [0 0 0; .3 .3 1; .4 .8 1; .5 1 .8]; %blue/green

for i = 3:6
  col = colors(i-2,:);
  b = 10^(i-1)
   
  % plot lines for CRP exact prediction using summation
  [logbins predicted dummy] = logbinmean(counts, crppred(counts,b),20,20);
  ph = plot(log10(logbins),log10(predicted));
  set(ph,'color',col,'linewidth',2);

  disp(['Loading results for b = ' num2str(b) ]);
  %%%  uncomment these lines if .mat file is not yet generated. %%%
  %typecountrecord= load([ 'outputs/typecountrecordwsjflat0.0.' num2str(b) '.0.dat']);
  %typecountrecordmean = mean(typecountrecord(:,:));
  %save([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  load([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat']);
  
  %plot emprical counts with error bars
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20);
  ph = plot(log10(logbins),log10(meanval));
  %set(ph,'color',col,'linestyle','o','markerfacecolor',col,'markersize',8);
  set(ph,'color',col,'linestyle','o','linewidth',2,'markersize',8);
  %errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');

  disp(['Loading results for b = ' num2str(b) ]);
  %%%  uncomment these lines if .mat file is not yet generated. %%%
  %typecountrecord= load([ 'outputs/typecountrecordwsjpeak0.0.' num2str(b) '.0.dat']);
  %typecountrecordmean = mean(typecountrecord(:,:));
  %save([ 'outputs/typecountrecordmeanwsjpeak0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  load([ 'outputs/typecountrecordmeanwsjpeak0.0.' num2str(b) '.0.mat']);

  %plot emprical counts with error bars
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20);
  ph = plot(log10(logbins),log10(meanval));
  %set(ph,'color',col,'linestyle','^','markerfacecolor',col,'markersize',8);
  set(ph,'color',col,'linestyle','^','linewidth',2,'markersize',8);
  %errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'ko');

end

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([.1:.1:1 2:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-.1 2.5])
set(gca,'FontSize',16)
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {'0.1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
    '1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
%title('Chinese restaurant process adaptor')
ylabel('Mean number of lexical entries (tables)')
xlabel('Word frequency (n_w)')
labs = {'Expectation','Empirical, fixed base','Empirical, inferred base'};
legend(labs,'Location','NorthWest')
box on
