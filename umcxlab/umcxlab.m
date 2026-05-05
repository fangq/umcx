function varargout = umcxlab(cfg)
% [flux, detp] = umcxlab(cfg)
%
% umcx MATLAB mex binding interface
%
% Author: Qianqian Fang <q.fang at neu.edu>
%
% example:
%   [flux, detp] = umcxlab(mcxcreate('cube60'))
%
% -- this function is part of the umcx project (https://github.com/fangq/umcx)
%

bjdata = uint8(mcx2json(cfg, '.jdb', 'compression', ''));
[varargout{1:nargout}] = umcx(bjdata);
