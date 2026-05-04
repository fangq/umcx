function varargout = umcxlab(cfg)
% [flux, detp] = umcxlab(cfg)
%
% example:
%   [flux, detp] = umcxlab(mcxcreate('cube60'))

bjdata = uint8(mcx2json(cfg, '.jdb', 'compression', ''));
[varargout{1:nargout}] = umcx(bjdata);
