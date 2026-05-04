function varargout = umcxlab(cfg)
% [flux, detp] = umcxlab(cfg)
%
% example:
%   [flux, detp] = umcxlab(mcxcreate('cube60'))

jsonfile = [tempname, '.json'];
mcx2json(cfg, jsonfile);
[varargout{1:nargout}] = umcx(fileread(jsonfile));