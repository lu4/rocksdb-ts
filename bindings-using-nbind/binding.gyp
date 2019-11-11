{
	"targets": [
		{
			"includes": [
				"auto.gypi"
			],
			"sources": [
				"hello.cc"
			],
			"conditions": [
				["OS=='mac'", {
					"xcode_settings": {
						"OTHER_LDFLAGS": [
							"-L/usr/local/Cellar/rocksdb/6.4.6/lib",
							"-lrocksdb"
						]
					}
				}],
			]
		},

	],
	"include_dirs": [
		"../deps/rocksdb/rocksdb/include"
	],
	"includes": [
		"auto-top.gypi"
	],
	"link_settings": {
		"libraries": [
			"-lrocksdb"
        ],
		"library_dirs": [
			"/usr/local/Cellar/rocksdb/6.4.6/lib"
		]

	}
}
