function coords2D = fliptransformCyl_NPparab(coords3D)

% create an output matrix of the same size as the input
% first two rows are x and y
% the third row indicates whether the location should be visible

coords2D = coords3D;

% [azimuth,elevation,r] =
% cart2sph(coords3D(1,:),coords3D(2,:),coords3D(3,:));

hypotxy = hypot(coords3D(1,:),coords3D(2,:));
% rmaze = hypot(hypotxy,coords3D(3,:));
elev = atan2(coords3D(3,:),hypotxy);
azimuth = atan2(coords3D(2,:),coords3D(1,:));

% height = tan(elevation)*5;
% azimuth = wrapToPi(azimuth);
visible = ~(azimuth<-pi/4 & azimuth>(-pi + pi/4));

xSign = sign(coords3D(1,:));
ySign = sign(coords3D(2,:));
azTemp = abs(azimuth);
azTemp(xSign==-1) = -azTemp(xSign==-1)+pi;

a = -0.125;
b = -1*tan(azTemp).*ySign;
c = 5;

x = ((-b-sqrt(b.^2 - 4.*a.*c))./(2.*a));
x = x.*xSign;

x(x<-20) = -20;
x(x>20) = 20;
y = a.*x.^2;

proj_dist = -y+20.6;
screenDist = hypot(x,y+5);
heightOnScreen = tan(elev).*screenDist;%(coords3D(3,:).*screenDist)./hypotxy;
projectedImageHeight = (proj_dist./1.3).*(9/16);

coords2D(1,:) = ((x.*(20.6./(-y+20.6)))./7.5).*(16/9);
coords2D(2,:) = -1.*(heightOnScreen./projectedImageHeight);
coords2D(2,coords2D(2,:)<-1) = -1;

coords2D(3,:) = visible;



%%
% % keyboard;