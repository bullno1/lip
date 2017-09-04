const path = require("path");
const webpack = require("webpack");

module.exports = function(env) {
  const plugins = [];
  const isProd = env.prod === 'true';
  if(isProd) {
      plugins.push(new webpack.LoaderOptionsPlugin({
        minimize: true,
        debug: false
      }));
      plugins.push(new webpack.optimize.UglifyJsPlugin({
        sourceMap: true,
        compress: {
          warnings: false,
          screw_ie8: true,
          conditionals: true,
          unused: true,
          comparisons: true,
          sequences: true,
          dead_code: true,
          evaluate: true,
          if_return: true,
          join_vars: true,
        },
        output: {
          comments: false,
        },
      }));
  }

  return {
    devtool: env.prod ? 'source-map' : 'eval',
    entry: './main.js',
    output: {
      filename: 'bundle.js',
      sourceMapFilename: 'bundle.js.map'
    },
    plugins,
    module: {
      rules: [{
        test: /\.jsx?$/,
        exclude: /node_modules/,
        use: ['babel-loader'],
      }, {
        test: /\.scss$/,
        use: ['style-loader', 'css-loader', 'sass-loader']
      }, {
        test: /\.css$/,
        loader: ['style-loader', 'css-loader']
      }]
    },
    devServer: {
      historyApiFallback: true,
      port: 8080,
      compress: isProd,
      stats: {
        assets: true,
        children: false,
        chunks: false,
        hash: false,
        modules: false,
        publicPath: false,
        timings: true,
        version: false,
        warnings: true,
        colors: {
          green: '\u001b[32m',
        }
      },
    },
    resolve: {
      extensions: ['.js', '.jsx'],
      modules: [ path.join(__dirname, 'src'), 'node_modules' ]
    }
  };
};
