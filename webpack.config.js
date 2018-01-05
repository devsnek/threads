const UglifyJSPlugin = require('uglifyjs-webpack-plugin');

module.exports = {
  entry: './lib/preload.js',
  output: {
    filename: './preload.build.js',
  },
  plugins: [
    new UglifyJSPlugin(),
  ],
  node: {
    global: false,
  },
};
