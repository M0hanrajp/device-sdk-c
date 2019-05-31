/* template implementation of an Edgex device service using C SDK */

/*
 * Copyright (c) 2018-2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/devsdk.h"

#include <unistd.h>
#include <signal.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); edgex_device_service_free (service); free (impl); return x.code; }

typedef struct template_driver
{
  iot_logger_t * lc;
} template_driver;


static volatile sig_atomic_t running = true;
static void inthandler (int i)
{
  running = (i != SIGINT);
}

static void dump_protocols (iot_logger_t *lc, const edgex_protocols *prots)
{
  for (const edgex_protocols *p = prots; p; p = p->next)
  {
    iot_log_debug (lc, " [%s] protocol:", p->name);
    for (const edgex_nvpairs *nv = p->properties; nv; nv = nv->next)
    {
      iot_log_debug (lc, "    %s = %s", nv->name, nv->value);
    }
  }
}

static void dump_attributes (iot_logger_t *lc, const edgex_nvpairs *attrs)
{
  for (const edgex_nvpairs *a = attrs; a; a = a->next)
  {
    iot_log_debug (lc, "    %s = %s", a->name, a->value);
  }
}

/* --- Initialize ---- */
/* Initialize performs protocol-specific initialization for the device
 * service.
 */
static bool template_init
(
  void *impl,
  struct iot_logger_t *lc,
  const edgex_nvpairs *config
)
{
  template_driver *driver = (template_driver *) impl;
  driver->lc = lc;
  iot_log_debug(driver->lc,"Init");
  return true;
}

/* ---- Discovery ---- */
/* Device services which are capable of device discovery should implement it
 * in this callback. It is called in response to a request on the
 * device service's discovery REST endpoint. New devices should be added using
 * the edgex_device_add_device() method
 */
static void template_discover (void *impl) {}

/* ---- Get ---- */
/* Get triggers an asynchronous protocol specific GET operation.
 * The device to query is specified by the protocols. nreadings is
 * the number of values being requested and defines the size of the requests
 * and readings arrays. For each value, the commandrequest holds information
 * as to what is being requested. The implementation of this method should
 * query the device accordingly and write the resulting value into the
 * commandresult.
 *
 * Note - In a commandrequest, the DeviceResource represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_get_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the location of the device to be accessed and log it */
  iot_log_debug(driver->lc, "GET on device:");
  dump_protocols (driver->lc, protocols);

  for (uint32_t i = 0; i < nreadings; i++)
  {
    /* Log the attributes for each requested resource */
    iot_log_debug (driver->lc, "  Requested reading %u:", i);
    dump_attributes (driver->lc, requests[i].attributes);
    /* Fill in a result regardless */
    readings[i].type = String;
    readings[i].value.string_result = strdup ("Template result");
  }
  return true;
}

/* ---- Put ---- */
/* Put triggers an asynchronous protocol specific SET operation.
 * The device to set values on is specified by the protocols.
 * nvalues is the number of values to be set and defines the size of the
 * requests and values arrays. For each value, the commandresult holds the
 * value, and the commandrequest holds information as to where it is to be
 * written. The implementation of this method should effect the write to the
 * device.
 *
 * Note - In a commandrequest, the DeviceResource represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_put_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the location of the device to be accessed and log it */
  iot_log_debug (driver->lc, "PUT on device:");
  dump_protocols (driver->lc, protocols);

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */
    /* Log the attributes */
    iot_log_debug (driver->lc, "  Requested device write %u:", i);
    dump_attributes (driver->lc, requests[i].attributes);
    switch (values[i].type)
    {
      case String:
        iot_log_debug (driver->lc, "  Value: %s", values[i].value.string_result);
        break;
      case Uint64:
        iot_log_debug (driver->lc, "  Value: %lu", values[i].value.ui64_result);
        break;
      case Bool:
        iot_log_debug (driver->lc, "  Value: %s", values[i].value.bool_result ? "true" : "false");
        break;
      /* etc etc */
      default:
      break;
    }
  }
  return true;
}

/* ---- Disconnect ---- */
/* Disconnect handles protocol-specific cleanup when a device is removed. */
static bool template_disconnect (void *impl, edgex_protocols *device)
{
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void template_stop (void *impl, bool force) {}


static void usage (void)
{
  printf ("Options: \n");
  printf ("   -h, --help           : Show this text\n");
  printf ("   -n, --name=<name>    : Set the device service name\n");
  printf ("   -r, --registry=<url> : Use the registry service\n");
  printf ("   -p, --profile=<name> : Set the profile name\n");
  printf ("   -c, --confdir=<dir>  : Set the configuration directory\n");
}

static bool testArg (int argc, char *argv[], int *pos, const char *pshort, const char *plong, char **var)
{
  if (strcmp (argv[*pos], pshort) == 0 || strcmp (argv[*pos], plong) == 0)
  {
    if (*pos < argc - 1)
    {
      (*pos)++;
      *var = argv[*pos];
      (*pos)++;
      return true;
    }
    else
    {
      printf ("Option %s requires an argument\n", argv[*pos]);
      exit (0);
    }
  }
  char *eq = strchr (argv[*pos], '=');
  if (eq)
  {
    if (strncmp (argv[*pos], pshort, eq - argv[*pos]) == 0 || strncmp (argv[*pos], plong, eq - argv[*pos]) == 0)
    {
      if (strlen (++eq))
      {
        *var = eq;
        (*pos)++;
        return true;
      }
      else
      {
        printf ("Option %s requires an argument\n", argv[*pos]);
        exit (0);
      }
    }
  }
  return false;
}

int main (int argc, char *argv[])
{
  char *profile = "";
  char *confdir = "";
  char *svcname = "device-template";
  char *regURL = getenv ("EDGEX_REGISTRY");

  template_driver * impl = malloc (sizeof (template_driver));
  memset (impl, 0, sizeof (template_driver));

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      usage ();
      return 0;
    }
    if (testArg (argc, argv, &n, "-r", "--registry", &regURL))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-n", "--name", &svcname))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-p", "--profile", &profile))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-c", "--confdir", &confdir))
    {
      continue;
    }

    printf ("Unknown option %s\n", argv[n]);
    usage ();
    return 0;
  }

  edgex_error e;
  e.code = 0;

  /* Device Callbacks */
  edgex_device_callbacks templateImpls =
  {
    template_init,         /* Initialize */
    template_discover,     /* Discovery */
    template_get_handler,  /* Get */
    template_put_handler,  /* Put */
    template_disconnect,   /* Disconnect */
    template_stop          /* Stop */
  };

  /* Initalise a new device service */
  edgex_device_service *service = edgex_device_service_new
    (svcname, "1.0", impl, templateImpls, &e);
  ERR_CHECK (e);

  /* Start the device service*/
  edgex_device_service_start (service, regURL, profile, confdir, &e);
  ERR_CHECK (e);

  signal (SIGINT, inthandler);
  running = true;
  while (running)
  {
    sleep(1);
  }

  /* Stop the device service */
  edgex_device_service_stop (service, true, &e);
  ERR_CHECK (e);

  edgex_device_service_free (service);
  free (impl);
  return 0;
}
