#!/usr/bin/env python
import sys
sys.path.append(sys.path[0])
from wforce import app
app.run(debug=True, use_reloader=False)
