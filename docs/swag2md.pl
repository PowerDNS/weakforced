use strict;
use warnings;
 
use Swagger2;
use Swagger2::Markdown;
 
my %pod_markdown_opts;

my $s2md = Swagger2::Markdown->new(
    swagger2 => Swagger2->new->load( $ARGV[0] )
);
 
my $api_bp_str   = $s2md->api_blueprint;
 
my $markdown_str = $s2md->markdown( %pod_markdown_opts );

my $progname = $ARGV[0];
$progname =~ /.*\/(.*)\.([1-9]).*/;
$progname = uc $1;
my $man_version = $2;
print "% $progname($man_version)\n";
print "% Open-Xchange\n";
print "% 2018\n\n";
print $markdown_str;
