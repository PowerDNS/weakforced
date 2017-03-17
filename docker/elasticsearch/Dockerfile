FROM elasticsearch:5.2
ENV ES_JAVA_OPTS="-Des.path.conf=/etc/elasticsearch"
CMD ["-E", "network.host=_site_", "-E", "discovery.zen.minimum_master_nodes=1"]
