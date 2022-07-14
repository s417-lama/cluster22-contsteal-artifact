#!/usr/bin/env python3

import sys
import os
from bottle import Bottle, static_file, debug, template
from livereload import Server

file_types = ["png", "svg", "pdf", "html", "md"]

html = """
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.5.0/css/bootstrap.min.css" integrity="sha384-9aIt2nRpC12Uk9gS9baDl411NQApFmC26EwAOH8WgZl5MYYxFfc+NcPb1dKGj7Sk" crossorigin="anonymous">
  </head>
  <body>
    <div class="container-fluid">
      <div class="row">
        <div class="col-3 px-3 py-3 position-fixed">
          <nav aria-label="breadcrumb">
            <ol class="breadcrumb">
              <li class="breadcrumb-item"><a href="/">{{ cwd }}</a></li>
              % for i in range(len(dirlist)):
                % path = "/".join(dirlist[:i+1])
                <li class="breadcrumb-item"><a href="/{{ path }}">{{ dirlist[i] }}</a></li>
              % end
            </ol>
          </nav>
          <div class="list-group" style="max-height: 90vh; overflow: auto;">
            % for file in filelist:
              % if file["isdir"]:
                <a class="list-group-item list-group-item-primary" href="/{{ "/".join([*dirlist, file["name"]]) }}">{{ file["name"] }}</a>
              % elif file["file_type"] in file_types:
                % if file["name"] == filename:
                  <a class="list-group-item active">{{ file["name"] }}</a>
                % else:
                  <a class="list-group-item" href="/{{ "/".join([*dirlist, file["name"]]) }}">{{ file["name"] }}</a>
                % end
              % end
            % end
          </div>
        </div>
        <div class="col offset-3">
          % if filename:
            <iframe src="/{{ "/".join(["static", *dirlist, filename]) }}" style="width: 100%; height: 99vh; position: relative;" frameborder="0"></iframe>
          % end
        </div>
      </div>
    </div>
  </body>
</html>
"""

debug(True)
app = Bottle()

def listdir_with_attr(path):
    return [dict(name=d, isdir=os.path.isdir(os.path.join(path, d)), file_type=d.split(".")[-1])
            for d in sorted(os.listdir(path))]

def get_dirlist(path):
    return [d for d in path.split(os.sep) if d]

@app.route("/static/<filepath:path>")
def server_static(filepath):
    return static_file(filepath, root=".")

@app.route("/<filepath:path>")
def show_file(filepath):
    cwd = os.path.basename(os.getcwd())
    if os.path.isdir(filepath):
        return template(html,
                        filelist   = listdir_with_attr(filepath),
                        cwd        = cwd,
                        dirlist    = get_dirlist(filepath),
                        filename   = "",
                        file_types = file_types)
    else:
        (dirname_r, filename) = os.path.split(filepath)
        (dirname_a, filename) = os.path.split(os.path.realpath(filepath))
        return template(html,
                        filelist   = listdir_with_attr(dirname_a),
                        cwd        = cwd,
                        dirlist    = get_dirlist(dirname_r),
                        filename   = filename,
                        file_types = file_types)

@app.route("/")
def index():
    cwd = os.path.basename(os.getcwd())
    return template(html,
                    filelist   = listdir_with_attr("."),
                    cwd        = cwd,
                    dirlist    = [],
                    filename   = "",
                    file_types = file_types)

server = Server(app)
for ft in file_types:
    server.watch("**/*.{}".format(ft))
server.serve(port=int(sys.argv[1]))
