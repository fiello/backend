namespace WebRole1
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Net.Http;
    using System.Web;
    using System.Web.Http;
    using System.Web.Mvc;
    using System.Web.Routing;
    using WebRole1.Controllers;
    
    public class RouteConfig
    {
        private const string DefaultRoute = RegistrarController.DefaultAction;

        public static void RegisterRoutes(RouteCollection routes)
        {
            MapRegistrarRoutes(routes, "Default route with user name", DefaultRoute + "/{userName}", new { controller = RegistrarController.Prefix, userName = RouteParameter.Optional} );

            MapRegistrarRoutes(routes, "Default route with action", DefaultRoute + "/{action}", new { });

            MapRegistrarRoutes(routes, "route to get registrations", DefaultRoute, new { controller = RegistrarController.Prefix, action = RegistrarController.DefaultAction });

            MapRegistrarRoutes(routes, "route to add registrations", DefaultRoute, new { controller = RegistrarController.Prefix, action = RegistrarController.RegisterAction });

            MapRegistrarRoutes(routes, "route to delete registrations", DefaultRoute + "/{userName}/{nodeId}", new { controller = RegistrarController.Prefix, action = RegistrarController.UnregisterAction });
        }

        private static void MapRegistrarRoutes(RouteCollection routes, string name, string route, object defaults)
        {
            routes.MapHttpRoute(name, route, defaults);
        }

    }
}
