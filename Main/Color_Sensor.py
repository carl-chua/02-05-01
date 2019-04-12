import RPi.GPIO as GPIO
import time

s2 = 23
s3 = 24
signal = 25
NUM_CYCLES = 20


def setup():
  GPIO.setmode(GPIO.BCM)
  GPIO.setup(signal,GPIO.IN, pull_up_down=GPIO.PUD_UP)
  GPIO.setup(s2,GPIO.OUT)
  GPIO.setup(s3,GPIO.OUT)
  print("\n")
  




def loop():
  temp = 10
  n = 5
  redtotal = 0
  bluetotal = 0
  greentotal = 0
  while(n):
    n = n-1
    GPIO.output(s2,GPIO.LOW)
    GPIO.output(s3,GPIO.LOW)
    time.sleep(0.1)
    start = time.time()
    for impulse_count in range(NUM_CYCLES):
      GPIO.wait_for_edge(signal, GPIO.FALLING)
    duration = time.time() - start      #seconds to run for loop
    red  = NUM_CYCLES / duration   #in Hz 11000 to 2000
    redtotal += (red-2000)/(9000/255)

    GPIO.output(s2,GPIO.LOW)
    GPIO.output(s3,GPIO.HIGH)
    time.sleep(0.1)
    start = time.time()
    for impulse_count in range(NUM_CYCLES):
      GPIO.wait_for_edge(signal, GPIO.FALLING)
    duration = time.time() - start
    blue = NUM_CYCLES / duration    #16000 to 2800
    bluetotal += (blue-2800)/(13200/255)


    GPIO.output(s2,GPIO.HIGH)
    GPIO.output(s3,GPIO.HIGH)
    time.sleep(0.1)
    start = time.time()
    for impulse_count in range(NUM_CYCLES):
      GPIO.wait_for_edge(signal, GPIO.FALLING)
    duration = time.time() - start
    green = NUM_CYCLES / duration  #11000 to 2000
    greentotal += (green - 2000)/(8500/255)
  if (redtotal > greentotal and redtotal > bluetotal):
     print("Red detected!")
  elif (greentotal > redtotal and greentotal > bluetotal):
     print("Green detected!")
  elif (bluetotal > greentotal and bluetotal > redtotal):
     print("Blue detected!")


def endprogram():
    GPIO.cleanup()

if __name__=='__main__':
    
    setup()

    try:
        loop()
        endprogram()
    except KeyboardInterrupt:
        endprogram()
