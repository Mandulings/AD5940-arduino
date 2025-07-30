clc; clear; close all;

deviceName = "SWVReader";
serviceUUID = "ecce822d-9357-480a-9c74-4e5e8322f3c7";

VoltageUUID = "bcdef012-1234-5678-1234-56789abcdef0";  % Voltage
CurrentUUID = "f968b639-9ec8-4d21-830a-1d0eaf49e76c";  % Current
FrequencyUUID = "8c5eee30-4cf2-46e8-a614-3eccc19ef130";
EndFlagUUID = "dbe10000-1234-5678-1234-56789abcdef0";  

global Voltage Current EndFlag
Voltage = [];
Current = [];
EndFlag = false;

devices = blelist("Name", deviceName);
disp(devices);

b = ble(deviceName);
b.Characteristics
disp('Connected to device');

c1 = characteristic(b, serviceUUID, VoltageUUID);
c2 = characteristic(b, serviceUUID, CurrentUUID);
c3 = characteristic(b, serviceUUID, FrequencyUUID);
c4 = characteristic(b, serviceUUID, EndFlagUUID);
    
c1.DataAvailableFcn = @(src, evt) readBLEandStack(src, 'Voltage');
c2.DataAvailableFcn = @(src, evt) readBLEandStack(src, 'Current');
c4.DataAvailableFcn = @(src, evt) endFlagHandler(src);

Frequency = typecast(uint8(read(c3)), 'single');

subscribe(c1, 'notification');
subscribe(c2, 'notification');
subscribe(c4, 'notification');

disp('Subscribed to characteristics');

while ~EndFlag
    pause(0.1);
end

disp('End flag received. Unsubscribing...');
unsubscribe(c1);
unsubscribe(c2);
unsubscribe(c4);

clear b c1 c2 c3 c4;
disp('Finished and cleared BLE objects');

% Save to Excel
len = min(numel(Voltage), numel(Current));

Index = (1:len)';
Voltage_Table = Voltage(1:len);
Current_Table = Current(1:len);
Time = Index / (2*Frequency) ;

T = table(Index, Voltage_Table, Current_Table);


filename = "SWV_Data_" + datestr(now, 'yyyymmdd_HHMMSS') + ".xlsx";
writetable(T, filename);

disp("Data saved to " + filename);


%%
f1 = figure;
stairs(Time, Voltage_Table);
xlabel("Time(s)");
ylabel("Voltage(V)");
title("DAC Ramp Voltage vs. Time")
grid on;


f2 = figure;
plot(Voltage_Table, Current_Table, '.');
xlabel("Voltage(V)");
ylabel("Current(uA)");
title("ADC Current vs. DAC Ramp Voltage");
grid on;

%% Function: Read and Stack per characteristic
function readBLEandStack(src, varName)
    global Voltage Current

    data = read(src, 'oldest');
    if mod(numel(data), 4) ~= 0
        disp('Invalid data size received');
        return;
    end

    floats = typecast(uint8(data), 'single');
    
    switch varName
        case 'Voltage'
            Voltage = [Voltage; floats(:)];
        case 'Current'
            Current = [Current; floats(:)];
    end

    assignin('base', varName, eval(varName));  % Update workspace
    disp(varName + " received " + num2str(numel(floats)) + " floats | Total: " + num2str(numel(eval(varName))));
end

function endFlagHandler(src)
    global EndFlag
    data = read(src, 'oldest');
    if ~isempty(data) && data(1) == 1
        EndFlag = true;
        disp('End flag detected!');
    end
end
