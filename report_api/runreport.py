#!/usr/bin/env python
import sys
sys.path.append(sys.path[0])
from wforce import app
if __name__ == "__main__":
	app.run(debug=True, use_reloader=False)
