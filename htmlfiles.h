INCBIN(legend_css, "html/legend.css");
INCBIN(local_js, "html/local.js");
INCBIN(graph_css, "html/graph.css");
INCBIN(detail_css, "html/detail.css");
INCBIN(index_html, "html/index.html");
INCBIN(lines_css, "html/lines.css");
INCBIN(js_underscore_min_js, "html/js/underscore-min.js");
INCBIN(js_moment_min_js, "html/js/moment.min.js");
INCBIN(js_jsrender_js, "html/js/jsrender.js");
INCBIN(js_jquery_1_8_3_min_js, "html/js/jquery-1.8.3.min.js");
INCBIN(js_d3_v3_js, "html/js/d3.v3.js");
INCBIN(js_rickshaw_min_js, "html/js/rickshaw.min.js");
INCBIN(js_purl_js, "html/js/purl.js");
map<string,string> g_urlmap={
{"legend.css", string((const char*)glegend_cssData, glegend_cssSize)},
{"local.js", string((const char*)glocal_jsData, glocal_jsSize)},
{"graph.css", string((const char*)ggraph_cssData, ggraph_cssSize)},
{"detail.css", string((const char*)gdetail_cssData, gdetail_cssSize)},
{"index.html", string((const char*)gindex_htmlData, gindex_htmlSize)},
{"lines.css", string((const char*)glines_cssData, glines_cssSize)},
{"js/underscore-min.js", string((const char*)gjs_underscore_min_jsData, gjs_underscore_min_jsSize)},
{"js/moment.min.js", string((const char*)gjs_moment_min_jsData, gjs_moment_min_jsSize)},
{"js/jsrender.js", string((const char*)gjs_jsrender_jsData, gjs_jsrender_jsSize)},
{"js/jquery-1.8.3.min.js", string((const char*)gjs_jquery_1_8_3_min_jsData, gjs_jquery_1_8_3_min_jsSize)},
{"js/d3.v3.js", string((const char*)gjs_d3_v3_jsData, gjs_d3_v3_jsSize)},
{"js/rickshaw.min.js", string((const char*)gjs_rickshaw_min_jsData, gjs_rickshaw_min_jsSize)},
{"js/purl.js", string((const char*)gjs_purl_jsData, gjs_purl_jsSize)},
};
