module.exports = {
  debug: true,
  devtool: 'inline-source-map',
  entry: './main.js',
  output: {
    filename: 'bundle.js',
    sourceMapFilename: 'bundle.js.map'
  },
  module: {
    loaders: [{
      test: /\.jsx?$/,
      exclude: /(node_modules|bower_components)/,
      loader: 'babel',
      query: {
        presets: ['es2015' ]
      }
    }, {
      test: /\.scss$/,
      loader: 'style!css!sass!'
    }, {
      test: /\.css$/,
      loader: 'style!css!'
    }]
  }
};
