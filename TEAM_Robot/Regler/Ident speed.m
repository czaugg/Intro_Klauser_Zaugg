%Identification of IntroSumoBot motor control

u = MotorDutyLeft;
y_m = SpeedLeft;
t = Time./1000000;
dt = 1/50;

plot(t, u, t, y_m)

x = find(Time >= 14000000, 1)
t = Time(1:x)./1000000;
u = (MotorDutyLeft(1:x).*(2^16/100)) - 20;
y_m = (SpeedLeft(1:x)) - 1320;

plot(t, u*50, '+' , t, y_m, '+')

x0 = 7.019;
xtu = 7.045;
xtg = 7.086;

K = 6750 / 5.2429e+04;
Tu = xtu-x0;
Tg = xtg - xtu;

Kp100 = 1.2/K*Tg/Tu * 100
Ti100 = 2*Tu * 100
Td100 = 0.5*Tu * 100