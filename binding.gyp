{
  "targets": [
    {
      "target_name": "gadu",
      "sources": [ "src/gadu.cc", "src/session.cc", "src/uv_resolver.cc" ],
      "libraries": [ "-lgadu" ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
